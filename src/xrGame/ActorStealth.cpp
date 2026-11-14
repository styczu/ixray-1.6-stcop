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
	float RangeNight = 20.f;
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

	bool TorchRevealsActor(CActor& Player)
	{
		return TorchReveals != 0 && ActorLightPerception::IsLightOn(Player);
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

float ActorStealth::VisualRange(CActor& Player)
{
	float Light = 1.f;
	if (!TorchRevealsActor(Player))
	{
		Light = Player.ROS() ? Player.ROS()->get_luminocity() : 1.f;
		clamp(Light, 0.f, 1.f);
	}
	return RangeNight + (RangeDay - RangeNight) * Light;
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
	CMD4(CCC_StealthFloat, "ai_stealth_range_night", &RangeNight, 1.f, 300.f);
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
}

// Called from CCC_RegisterCommands (console_commands.cpp).
void RegisterActorStealthConsoleCommands()
{
	ActorStealth::RegisterConsoleCommands();
}
