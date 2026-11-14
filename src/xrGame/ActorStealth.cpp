#include "StdAfx.h"
#include "ActorStealth.h"
#include "Actor.h"
#include "ActorLightPerception.h"
#include "../xrEngine/xr_ioc_cmd.h"
#include "../xrEngine/XR_IOConsole.h"

namespace
{
	// Values come from the mod options menu (ixray-npc-stealth) and are not saved to user.ltx.
	int StealthEnabled = 0;
	float RangeDay = 100.f;
	// Night range by the rank of the observer (user model 2026-11-14).
	float RangeNovice = 20.f;
	float RangeExperienced = 30.f;
	float RangeVeteran = 45.f;
	float RangeMaster = 100.f;
	float MoveStill = 0.25f;
	float MoveSneak = 0.5f;
	float MoveWalk = 1.f;
	float MoveRun = 2.f;
	float MoveSprint = 3.f;
	float StanceCrouch = 0.6f;
	float StanceCrouchLow = 0.4f;
	float Peripheral = 0.3f;
	float Alert = 3.f;
	int TorchReveals = 1;
	// How long a hit by the actor keeps the vanilla vision, as DANGER_INERTION_TIME in xr_danger.script.
	const u32 HitMemoryTime = 30000;
	// npc-hearing-actor-shots: suppressed shot hearing ranges (user model 2026-11-14: pistol 10, shotgun and
	// carbine 20, rifle 50 m, half of it in the day) and the window in which a heard shot still counts.
	float SilencerPistol = 10.f;
	float SilencerCarbine = 20.f;
	float SilencerRifle = 50.f;
	float SilencerDay = 0.5f;
	const u32 ShotFreshTime = 2000;

	bool TorchRevealsActor(CActor& Player)
	{
		return TorchReveals != 0 && ActorLightPerception::IsLightOn(Player);
	}

	// Brightness by time of day (user model 2026-11-14): day 1, evening and dawn 0.55, night by weather
	// (bright moonlit ~0.2, dark ~0), 0.75 of it in the sun shadow. Defaults equal mod_system_npc_stealth.ltx.
	struct CurvePoint
	{
		float Hour;
		float Value; // NightValue = the night level of the weather
	};
	const float NightValue = -1.f;
	const int MaxCurvePoints = 24;
	CurvePoint Curve[MaxCurvePoints] = {
		{4.5f, NightValue}, {5.5f, 0.55f}, {6.5f, 1.f}, {20.f, 1.f}, {22.f, 0.55f}, {23.5f, NightValue},
	};
	int CurveCount = 6;
	float ShadeFactor = 0.75f;
	float NightDefault = 0.1f;
	xr_vector<std::pair<xr_string, float>> NightLevels = {
		{"default_cloudy", 0.2f}, {"default_pure", 0.2f}, {"default_rain", 0.17f}, {"default_white", 0.15f},
		{"default_clear", 0.13f}, {"default_dark", 0.1f}, {"default_drops", 0.1f}, {"default_thunder", 0.06f},
		{"default_storm", 0.f}, {"default_fog", 0.f},
	};
	xr_vector<xr_string> RawLightWeathers = {"indoor", "indoor_ambient", "default_way_to_pripyat"};

	// Smoothing: the night level follows a weather change within ~10 s, the shade ~0.5 s.
	const float NightBlendTime = 10.f;
	const float ShadeBlendTime = 0.5f;
	const float ShadeCheckPeriod = 0.25f;
	bool ConfigLoaded = false;
	bool LightStarted = false;
	float LastTime = 0.f;
	float NextShadeCheck = 0.f;
	float NightNow = 0.f;
	float ShadeNow = 0.f;
	float ShadeTarget = 0.f;

	void EnsureConfig()
	{
		if (ConfigLoaded)
			return;
		ConfigLoaded = true;
		ActorStealthWorld::LoadConfig();
	}

	const char* SkipSpaces(const char* Text)
	{
		while (*Text == ' ' || *Text == '\t')
			++Text;
		return Text;
	}

	// Next comma-separated item without surrounding spaces; false at the end of the text.
	bool NextItem(const char*& Text, xr_string& Item)
	{
		Text = SkipSpaces(Text);
		if (!*Text)
			return false;
		const char* End = Text;
		while (*End && *End != ',')
			++End;
		const char* Last = End;
		while (Last > Text && (Last[-1] == ' ' || Last[-1] == '\t'))
			--Last;
		Item.assign(Text, Last - Text);
		Text = *End ? End + 1 : End;
		return true;
	}

	bool ParseNumber(const xr_string& Item, float& Value)
	{
		char* End = nullptr;
		Value = strtof(Item.c_str(), &End);
		return !Item.empty() && End && !*End;
	}

	class CCC_StealthFloat : public CCC_Float
	{
	public:
		CCC_StealthFloat(const char* N, float* V, float Min, float Max) : CCC_Float(N, V, Min, Max) {}
		virtual void Save(IWriter*) {}
	};

	class CCC_StealthInteger : public CCC_Integer
	{
	public:
		CCC_StealthInteger(const char* N, int* V, int Min, int Max) : CCC_Integer(N, V, Min, Max) {}
		virtual void Save(IWriter*) {}
	};
}

bool ActorStealth::Enabled()
{
	return StealthEnabled != 0;
}

float ActorStealth::VisualRange(CActor& Player, int ObserverRank)
{
	const float Night = NightRange(ActorStealthWorld::RankName(ObserverRank));
	return Night + (RangeDay - Night) * Brightness(Player);
}

float ActorStealth::SilencedShotRange(const char* WeaponClass, float Hours)
{
	float Range = SilencerCarbine;
	if (!WeaponClass || !*WeaponClass)
		Range = SilencerPistol;
	else if (!xr_strcmp(WeaponClass, "sniper_rifle") || !xr_strcmp(WeaponClass, "heavy_weapon"))
		Range = SilencerRifle;
	return Range * (1.f - (1.f - SilencerDay) * DayCurve(Hours, 0.f));
}

bool ActorStealth::ShotStillFresh(u32 HeardTime, u32 Now)
{
	// A time ahead of Now (stored before a level reload) wraps around and does not count.
	return HeardTime != 0 && Now - HeardTime <= ShotFreshTime;
}

float ActorStealth::NightRange(const char* Rank)
{
	if (Rank)
	{
		if (!xr_strcmp(Rank, "experienced"))
			return RangeExperienced;
		if (!xr_strcmp(Rank, "veteran"))
			return RangeVeteran;
		if (!xr_strcmp(Rank, "master"))
			return RangeMaster;
	}
	return RangeNovice;
}

float ActorStealth::Brightness(CActor& Player)
{
	EnsureConfig();
	if (TorchRevealsActor(Player))
		return 1.f;

	float Raw = Player.ROS() ? Player.ROS()->get_luminocity() : 1.f;
	clamp(Raw, 0.f, 1.f);
	const char* Weather = ActorStealthWorld::Weather();
	if (UsesRawLight(Weather))
		return Raw;

	const float Now = ActorStealthWorld::Seconds();
	const float Night = NightLevel(Weather);
	if (!LightStarted || Now < LastTime)
	{
		LightStarted = true;
		LastTime = Now;
		NightNow = Night;
		ShadeTarget = ShadeNow = ActorStealthWorld::SunBlocked(Player) ? 1.f : 0.f;
		NextShadeCheck = Now + ShadeCheckPeriod;
	}
	const float Delta = Now - LastTime;
	LastTime = Now;
	if (Now >= NextShadeCheck)
	{
		ShadeTarget = ActorStealthWorld::SunBlocked(Player) ? 1.f : 0.f;
		NextShadeCheck = Now + ShadeCheckPeriod;
	}
	NightNow += (Night - NightNow) * std::min(1.f, Delta / NightBlendTime);
	ShadeNow += (ShadeTarget - ShadeNow) * std::min(1.f, Delta / ShadeBlendTime);

	float Light = DayCurve(ActorStealthWorld::GameHours(), NightNow) * (1.f + (ShadeFactor - 1.f) * ShadeNow);
	Light = std::max(Light, Raw);
	clamp(Light, 0.f, 1.f);
	return Light;
}

float ActorStealth::DayCurve(float Hours, float Night)
{
	if (CurveCount <= 0)
		return 1.f;
	Hours = fmodf(Hours, 24.f);
	if (Hours < 0.f)
		Hours += 24.f;
	for (int Index = 0; Index < CurveCount; ++Index)
	{
		const CurvePoint& From = Curve[Index];
		const CurvePoint& To = Curve[(Index + 1) % CurveCount];
		float Start = From.Hour, End = To.Hour, At = Hours;
		if (End <= Start)
			End += 24.f; // the segment crosses midnight (or the curve has one point)
		if (At < Start)
			At += 24.f;
		if (At > End)
			continue;
		const float A = From.Value == NightValue ? Night : From.Value;
		const float B = To.Value == NightValue ? Night : To.Value;
		return A + (B - A) * (At - Start) / (End - Start);
	}
	return 1.f;
}

float ActorStealth::NightLevel(const char* Weather)
{
	if (Weather)
	{
		for (const auto& Level : NightLevels)
		{
			if (Level.first == Weather)
				return Level.second;
		}
	}
	return NightDefault;
}

bool ActorStealth::UsesRawLight(const char* Weather)
{
	if (!Weather)
		return false;
	for (const xr_string& Name : RawLightWeathers)
	{
		if (Name == Weather)
			return true;
	}
	return false;
}

bool ActorStealth::SetCurve(const char* Text)
{
	CurvePoint Points[MaxCurvePoints];
	int Count = 0;
	xr_string Hour, Value;
	while (NextItem(Text, Hour))
	{
		if (Count == MaxCurvePoints || !NextItem(Text, Value))
			return false;
		CurvePoint& Point = Points[Count];
		if (!ParseNumber(Hour, Point.Hour) || Point.Hour < 0.f || Point.Hour >= 24.f)
			return false;
		if (Count && Point.Hour <= Points[Count - 1].Hour)
			return false;
		if (Value == "night")
			Point.Value = NightValue;
		else if (!ParseNumber(Value, Point.Value) || Point.Value < 0.f || Point.Value > 1.f)
			return false;
		++Count;
	}
	if (!Count)
		return false;
	for (int Index = 0; Index < Count; ++Index)
		Curve[Index] = Points[Index];
	CurveCount = Count;
	return true;
}

void ActorStealth::SetShade(float Factor)
{
	clamp(Factor, 0.f, 1.f);
	ShadeFactor = Factor;
}

void ActorStealth::SetNightDefault(float Value)
{
	clamp(Value, 0.f, 1.f);
	NightDefault = Value;
}

void ActorStealth::SetNight(const char* Weather, float Value)
{
	clamp(Value, 0.f, 1.f);
	for (auto& Level : NightLevels)
	{
		if (Level.first == Weather)
		{
			Level.second = Value;
			return;
		}
	}
	NightLevels.emplace_back(Weather, Value);
}

void ActorStealth::SetRawLightWeathers(const char* List)
{
	RawLightWeathers.clear();
	xr_string Name;
	while (NextItem(List, Name))
	{
		if (!Name.empty())
			RawLightWeathers.push_back(Name);
	}
}

float ActorStealth::RateFactor(CActor& Player)
{
	const u32 State = Player.GetMovementState(ACTOR_DEFS::eReal);
	const bool Torch = TorchRevealsActor(Player);
	const bool Crouch = (State & mcCrouch) != 0;
	const bool Accelerated = isActorAccelerated(State, Player.IsZoomAimingMode());

	// A lit torch removes the still/sneak and stance bonuses: the actor counts as walking upright.
	float Move = MoveWalk;
	if (State & mcSprint)
		Move = MoveSprint;
	else if (!(State & mcAnyMove))
		Move = Torch ? MoveWalk : MoveStill;
	else if (Crouch)
		Move = Torch ? MoveWalk : MoveSneak;
	else
		Move = Accelerated ? MoveRun : MoveWalk;

	if (Torch || !Crouch)
		return Move;

	// Low crouch uses the same condition as CActor::set_state_box (box 2).
	return Move * (Accelerated ? StanceCrouch : StanceCrouchLow);
}

float ActorStealth::PeripheralFactor(float AlphaRatio)
{
	clamp(AlphaRatio, 0.f, 1.f);
	return 1.f + (Peripheral - 1.f) * AlphaRatio;
}

float ActorStealth::AlertFactor()
{
	return Alert;
}

bool ActorStealth::FightsActor(bool SelectedActor, bool LastHitByActor, u32 LastHitTime, u32 Now)
{
	if (SelectedActor)
		return true;
	// A time ahead of Now (a hit stored before a level reload) wraps around and does not count.
	return LastHitByActor && (Now - LastHitTime < HitMemoryTime);
}

void ActorStealth::RegisterConsoleCommands()
{
	CMD4(CCC_StealthInteger, "ai_stealth", &StealthEnabled, 0, 1);
	CMD4(CCC_StealthFloat, "ai_stealth_range_day", &RangeDay, 1.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_range_novice", &RangeNovice, 1.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_range_experienced", &RangeExperienced, 1.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_range_veteran", &RangeVeteran, 1.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_range_master", &RangeMaster, 1.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_still", &MoveStill, 0.f, 10.f);
	CMD4(CCC_StealthFloat, "ai_stealth_sneak", &MoveSneak, 0.f, 10.f);
	CMD4(CCC_StealthFloat, "ai_stealth_walk", &MoveWalk, 0.f, 10.f);
	CMD4(CCC_StealthFloat, "ai_stealth_run", &MoveRun, 0.f, 10.f);
	CMD4(CCC_StealthFloat, "ai_stealth_sprint", &MoveSprint, 0.f, 10.f);
	CMD4(CCC_StealthFloat, "ai_stealth_crouch", &StanceCrouch, 0.f, 2.f);
	CMD4(CCC_StealthFloat, "ai_stealth_crouch_low", &StanceCrouchLow, 0.f, 2.f);
	CMD4(CCC_StealthFloat, "ai_stealth_peripheral", &Peripheral, 0.f, 1.f);
	CMD4(CCC_StealthFloat, "ai_stealth_alert", &Alert, 0.f, 20.f);
	CMD4(CCC_StealthInteger, "ai_stealth_torch", &TorchReveals, 0, 1);
	CMD4(CCC_StealthFloat, "ai_stealth_silencer_pistol", &SilencerPistol, 0.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_silencer_carbine", &SilencerCarbine, 0.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_silencer_rifle", &SilencerRifle, 0.f, 300.f);
	CMD4(CCC_StealthFloat, "ai_stealth_silencer_day", &SilencerDay, 0.f, 1.f);
}

// Called from CCC_RegisterCommands (console_commands.cpp).
void RegisterActorStealthConsoleCommands()
{
	ActorStealth::RegisterConsoleCommands();
}
