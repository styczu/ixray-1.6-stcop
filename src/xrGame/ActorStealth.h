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
	// Reads [npc_stealth_light] and [npc_stealth_light_night] (ixray-npc-stealth) when they exist.
	void LoadConfig();
}

// Stealth model for how stalkers see the actor (npc-vision-actor-stealth).
// Disabled by default: with ai_stealth 0 the vanilla visibility formula is used.
namespace ActorStealth
{
	bool Enabled();
	// Distance at which a stalker can notice the actor straight ahead,
	// interpolated between night and day ranges by the actor brightness.
	float VisualRange(CActor& Player);
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
