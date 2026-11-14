#include "StdAfx.h"
#include "ActorStealth.h"
#include "Actor.h"
#include "Level.h"
#include "GamePersistent.h"
#include "ui/UIInventoryUtilities.h"
#include "Inventory.h"
#include "Weapon.h"

// Engine side of the stealth model (npc-vision-actor-stealth): weather clock, weather name, sun shadow
// and the configuration from the ixray-npc-stealth addon.
namespace
{
	const char* LightSection = "npc_stealth_light";
	const char* NightSection = "npc_stealth_light_night";
}

float ActorStealthWorld::GameHours()
{
	return g_pGamePersistent->Environment().GetGameTime() / 3600.f;
}

const char* ActorStealthWorld::Weather()
{
	const char* Name = g_pGamePersistent->Environment().CurrentWeatherName.c_str();
	return Name ? Name : "";
}

bool ActorStealthWorld::SunBlocked(CActor& Player)
{
	// Same test as the sun term of the actor ROS (CROS_impl::calc_sun_value), static geometry only.
	Fvector Direction = g_pGamePersistent->Environment().CurrentEnv->sun_dir;
	Direction.invert();
	Fvector Position;
	Player.Center(Position);
	return Level().ObjectSpace.RayTest(Position, Direction, 500.f, collide::rqtStatic, nullptr, &Player);
}

float ActorStealthWorld::Seconds()
{
	return Device.fTimeGlobal;
}

const char* ActorStealthWorld::RankName(int Rank)
{
	return InventoryUtilities::GetRankAsText(Rank);
}

bool ActorStealthWorld::ShotHeard(CActor& Player, float Distance)
{
	PIItem Item = Player.inventory().ActiveItem();
	CWeapon* Weapon = Item ? Item->cast_weapon() : nullptr;
	if (!Weapon || !Weapon->IsSilencerAttached() || Weapon->IsGrenadeMode())
		return true;
	const char* Class = "";
	const shared_str& Section = Weapon->cNameSect();
	if (pSettings->line_exist(Section, "weapon_class"))
	{
		const char* Value = pSettings->r_string(Section, "weapon_class");
		if (Value)
			Class = Value;
	}
	return Distance <= ActorStealth::SilencedShotRange(Class, GameHours());
}

void ActorStealthWorld::LoadConfig()
{
	if (pSettings->section_exist(LightSection))
	{
		if (pSettings->line_exist(LightSection, "curve") && !ActorStealth::SetCurve(pSettings->r_string(LightSection, "curve")))
			Msg("! [%s] invalid curve, the engine default is used", LightSection);
		if (pSettings->line_exist(LightSection, "shade"))
			ActorStealth::SetShade(pSettings->r_float(LightSection, "shade"));
		if (pSettings->line_exist(LightSection, "night_default"))
			ActorStealth::SetNightDefault(pSettings->r_float(LightSection, "night_default"));
		if (pSettings->line_exist(LightSection, "raw_light_weathers"))
			ActorStealth::SetRawLightWeathers(pSettings->r_string(LightSection, "raw_light_weathers"));
	}
	if (pSettings->section_exist(NightSection))
	{
		for (const CInifile::Item& Item : pSettings->r_section(NightSection).Data)
			ActorStealth::SetNight(Item.first.c_str(), (float)atof(Item.second.c_str()));
	}
}
