#pragma once

class CActor;

// Stealth model for how stalkers see the actor (npc-vision-actor-stealth).
// Disabled by default: with ai_stealth 0 the vanilla visibility formula is used.
namespace ActorStealth
{
	bool Enabled();
	// Distance at which a stalker can notice the actor straight ahead,
	// interpolated between night and day ranges by the actor brightness.
	float VisualRange(CActor& Player);
	// Detection speed factor from movement mode and stance.
	float RateFactor(CActor& Player);
	// Detection speed factor from the angle off the observer's gaze (0 = straight ahead, 1 = FOV edge).
	float PeripheralFactor(float AlphaRatio);
	// Detection speed factor of an alerted (danger) observer relative to a calm one.
	float AlertFactor();
	void RegisterConsoleCommands();
}
