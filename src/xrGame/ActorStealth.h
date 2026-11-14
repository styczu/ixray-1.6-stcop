#pragma once

class CActor;

// Engine queries of the stealth model (ActorStealthWorld.cpp), kept apart so the model can be tested alone.
namespace ActorStealthWorld
{
	// Weather clock in hours, 0..24.
	float GameHours();
	// Current weather name (level.set_weather), "" when none.
	const char* Weather();
	// Static geometry blocks the way from the actor to the sun.
	bool SunBlocked(CActor& Player);
	// Real time in seconds, for smoothing.
	float Seconds();
	// Rank name of a character rank value (game_relations.ltx rating: novice, experienced, veteran, master).
	const char* RankName(int Rank);
	// npc-hearing-actor-shots: a stalker at Distance hears the shot of the actor's active weapon. A suppressed
	// shot (not in grenade launcher mode) reaches only ActorStealth::SilencedShotRange of the weapon_class of
	// the weapon section; any other shot keeps the range of its sound files.
	bool ShotHeard(CActor& Player, float Distance);
	// Reads [npc_stealth_light] and [npc_stealth_light_night] (ixray-npc-stealth) when they exist.
	void LoadConfig();
}

// Stealth model for how stalkers see the actor (npc-vision-actor-stealth).
// Disabled by default: with ai_stealth 0 the vanilla visibility formula is used.
namespace ActorStealth
{
	bool Enabled();
	// Distance at which a stalker of rank ObserverRank can notice the actor straight ahead:
	// N + (day range - N) x brightness, N = the night range of the rank.
	float VisualRange(CActor& Player, int ObserverRank);
	// Night range of a rank name (novice for an unknown one).
	float NightRange(const char* Rank);
	// npc-hearing-actor-shots: hearing range of a suppressed shot by weapon_class ("" = pistol: ai_stealth_
	// silencer_pistol; shotgun, assault_rifle and unknown: _carbine; sniper_rifle, heavy_weapon: _rifle), times
	// 1 - (1 - ai_stealth_silencer_day) x the day curve (full day: x0.5, night: x1).
	float SilencedShotRange(const char* WeaponClass, float Hours);
	// A heard shot of the actor (HeardTime in ms, 0 = none) still makes a hostile listener target him: within
	// 2 s, so a shot that turns the listener hostile (a hit on his group) counts as well.
	bool ShotStillFresh(u32 HeardTime, u32 Now);
	// Actor brightness 0..1 for the sight range: day curve of the weather clock with the night level of
	// the weather, times the shade factor in the sun shadow, at least the actor ROS (level lamps);
	// 1 with a lit torch. Underground weathers use the ROS alone.
	float Brightness(CActor& Player);
	// Day curve at Hours, Night standing for the "night" points.
	float DayCurve(float Hours, float Night);
	// Night level of a weather (night_default when it is not listed).
	float NightLevel(const char* Weather);
	// The weather has no day cycle (underground): brightness = actor ROS.
	bool UsesRawLight(const char* Weather);
	// Configuration, from ActorStealthWorld::LoadConfig. SetCurve takes "hour, value|night, ..." with
	// hours rising within 0..24 and keeps the previous curve on a malformed text (returns false).
	bool SetCurve(const char* Text);
	void SetShade(float Factor);
	void SetNightDefault(float Value);
	void SetNight(const char* Weather, float Value);
	void SetRawLightWeathers(const char* List);
	// Detection speed factor from movement mode and stance.
	float RateFactor(CActor& Player);
	// Detection speed factor from the angle off the observer's gaze (0 = straight ahead, 1 = FOV edge).
	float PeripheralFactor(float AlphaRatio);
	// Detection speed factor of an alerted (danger) observer relative to a calm one.
	float AlertFactor();
	// The observer has selected the actor as its enemy, or was hit by him less than 30 s ago
	// (LastHitTime and Now in ms, Device.dwTimeGlobal): it sees him with the vanilla formula.
	bool FightsActor(bool SelectedActor, bool LastHitByActor, u32 LastHitTime, u32 Now);
	void RegisterConsoleCommands();
}
