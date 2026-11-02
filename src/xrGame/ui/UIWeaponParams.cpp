#include "StdAfx.h"
#include "UIWeaponParams.h"
#include "UIConditionFormat.h"
#include "UIInventoryUtilities.h"
#include "../WeaponUiValues.h"
#include "../Level.h"
#include "game_base_space.h"
#include "../game_cl_single.h"
#include "inventory_item_object.h"
#include "Weapon.h"
#include "WeaponMagazinedWGrenade.h"
#include "WeaponShotgun.h"
#include "WeaponAutomaticShotgun.h"
#include "weaponBM16.h"
#include "WeaponBinoculars.h"
#include "WeaponKnife.h"
#include "Silencer.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/UIHelper.h"
#include "../../xrUI/ui_base.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIDoubleProgressBar.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrEngine/string_table.h"
#include "../../xrEngine/xr_input.h"

static LPCSTR weapon_bar_node[] =
{
	"damage",
	"penetration",
	"accuracy",
	"handling",
	"recoil",
	"hold",
	"reliability",
	"durability",
};

static LPCSTR weapon_bar_caption[] =
{
	"ui_uip_wpn_damage",
	"ui_uip_wpn_penetration",
	"ui_uip_wpn_accuracy",
	"ui_uip_wpn_handling",
	"ui_uip_wpn_recoil",
	"ui_uip_wpn_hold",
	"ui_uip_wpn_reliability",
	"ui_uip_wpn_durability",
};

static LPCSTR weapon_row_caption[] =
{
	"ui_uip_wpn_ammo",
	"ui_uip_wpn_capacity_mag",
	"ui_uip_wpn_rpm",
	"ui_uip_wpn_speed",
	"ui_uip_wpn_dispersion",
	"ui_uip_wpn_range",
	"ui_uip_wpn_condition",
};

CUIWeaponParams::CUIWeaponParams()
{
}

CUIWeaponParams::~CUIWeaponParams()
{
	xr_delete(m_Prop_line);
	xr_delete(m_compare);
	for (CUIStatic*& separator : m_separators)
		xr_delete(separator);
	xr_delete(m_condition.wnd);
	for (u32 i = 0; i < eBarCount; ++i)
		xr_delete(m_bars[i].wnd);
	for (u32 i = 0; i < eRowCount; ++i)
		xr_delete(m_rows[i].wnd);
	for (CUIStatic*& icon : m_ammo_icons)
		xr_delete(icon);
}

// <weapon_params gap=""> holds a <row> template (caption, value), one node per
// bar (<damage>, <penetration>, <accuracy>, <handling>, <recoil>, <hold>, <reliability>,
// <durability>: optional icon, caption, progress as a double bar with
// color_less/color_more, overflow, overflow_fill, value; a bar without its node
// is left out), the wear group's <condition> (optional icon, caption, progress
// with min_color/middle_color/max_color, value; without it the condition is a
// row), an optional <separator> after the gap between the groups and
// <ammo_icons x="" cell="" gap=""> (cell: the height of one inventory grid
// cell) with a <dim> colour for the ammo the numbers are not for. Optional
// <delta> nodes (in <row> and in each bar, right aligned text, one column) print the
// difference from the compared weapon, in the colours of <delta_better> and
// <delta_worse> (ui-weapon-compare-numbers).
void CUIWeaponParams::InitFromXml(CUIXml& xml)
{
	LPCSTR base = "weapon_params";
	XML_NODE* stored_root = xml.GetLocalRoot();
	XML_NODE* base_node = xml.NavigateToNode(base, 0);
	if (!base_node)
		return;

	CUIXmlInit::InitWindow(xml, base, 0, this);
	m_gap = xml.ReadAttribFlt(base, 0, "gap", 0.0f);
	xml.SetLocalRoot(base_node);

	if (xml.NavigateToNode("prop_line"))
	{
		m_Prop_line = UIHelper::CreateStatic(xml, "prop_line", nullptr);
		m_Prop_line->SetAutoDelete(false);
	}
	if (xml.NavigateToNode("compare"))
	{
		m_compare = UIHelper::CreateStatic(xml, "compare", nullptr);
		m_compare->SetAutoDelete(false);
	}
	if (xml.NavigateToNode("separator"))
	{
		for (CUIStatic*& separator : m_separators)
		{
			separator = UIHelper::CreateStatic(xml, "separator", nullptr);
			separator->SetAutoDelete(false);
		}
	}

	for (u32 i = 0; i < eRowCount; ++i)
	{
		SRow& row = m_rows[i];
		row.wnd = new CUIWindow();
		row.wnd->SetAutoDelete(false);
		CUIXmlInit::InitWindow(xml, "row", 0, row.wnd);
		row.caption = UIHelper::CreateStatic(xml, "row:caption", row.wnd);
		row.caption->SetTextST(weapon_row_caption[i]);
		row.value = UIHelper::CreateStatic(xml, "row:value", row.wnd);
		if (xml.NavigateToNode("row:delta"))
			row.delta = UIHelper::CreateStatic(xml, "row:delta", row.wnd);
	}

	m_delta_better_color = CUIXmlInit::GetColor(xml, "delta_better", 0, color_rgba(107, 207, 119, 255));
	m_delta_worse_color = CUIXmlInit::GetColor(xml, "delta_worse", 0, color_rgba(196, 18, 18, 255));

	string256 buf;
	if (xml.NavigateToNode("condition"))
	{
		m_condition.wnd = new CUIWindow();
		m_condition.wnd->SetAutoDelete(false);
		CUIXmlInit::InitWindow(xml, "condition", 0, m_condition.wnd);
		if (xml.NavigateToNode("condition:icon"))
			UIHelper::CreateStatic(xml, "condition:icon", m_condition.wnd);
		UIHelper::CreateStatic(xml, "condition:caption", m_condition.wnd)->SetTextST(weapon_row_caption[eRowCondition]);
		m_condition.bar = UIHelper::CreateProgressBar(xml, "condition:progress", m_condition.wnd);
		m_condition.value = UIHelper::CreateStatic(xml, "condition:value", m_condition.wnd);
	}

	for (u32 i = 0; i < eBarCount; ++i)
	{
		SBar& bar = m_bars[i];
		LPCSTR node = weapon_bar_node[i];
		if (!xml.NavigateToNode(node))
			continue;
		bar.wnd = new CUIWindow();
		bar.wnd->SetAutoDelete(false);
		CUIXmlInit::InitWindow(xml, node, 0, bar.wnd);
		xr_strconcat(buf, node, ":icon");
		if (xml.NavigateToNode(buf))
			UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":caption");
		UIHelper::CreateStatic(xml, buf, bar.wnd)->SetTextST(weapon_bar_caption[i]);
		bar.bar = new CUIDoubleProgressBar();
		bar.wnd->AttachChild(bar.bar);
		bar.bar->SetAutoDelete(true);
		xr_strconcat(buf, node, ":progress");
		bar.bar->InitFromXml(xml, buf);
		xr_strconcat(buf, node, ":overflow");
		if (xml.NavigateToNode(buf))
			bar.overflow = UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":overflow_fill");
		if (xml.NavigateToNode(buf))
			bar.overflow_fill = UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":value");
		bar.value = UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":delta");
		if (xml.NavigateToNode(buf))
			bar.delta = UIHelper::CreateStatic(xml, buf, bar.wnd);
	}
	m_bars[eBarDamage].full = WeaponUi::DamageScore(WeaponUi::DamageFull);

	if (xml.NavigateToNode("ammo_icons"))
	{
		m_icons_x = xml.ReadAttribFlt("ammo_icons", 0, "x", 0.0f);
		m_icons_gap = xml.ReadAttribFlt("ammo_icons", 0, "gap", 0.0f);
		m_icons_cell = xml.ReadAttribFlt("ammo_icons", 0, "cell", 40.0f);
		m_icon_dim_color = CUIXmlInit::GetColor(xml, "ammo_icons:dim", 0, color_rgba(255, 255, 255, 80));
	}

	xml.SetLocalRoot(stored_root);
}

// The stock weapon tooltip's rule (CUIWpnParams::Check): fake and melee
// weapons, silencers, binoculars and knives have no weapon parameters.
bool CUIWeaponParams::Check(CInventoryItem& item)
{
	if (!m_bars[eBarDurability].wnd || !item.cast_weapon())
		return false;
	LPCSTR sect = item.object().cNameSect().c_str();
	if (!pSettings->line_exist(sect, "fire_dispersion_base"))
		return false;
	if (pSettings->line_exist(sect, "ammo_mag_size") && pSettings->r_u32(sect, "ammo_mag_size") == 0)
		return false;
	return !item.cast_addon_silencer() && !item.cast_weapon_binoculars() && !item.cast_weapon_knife();
}

CUIWeaponParams::SAmmo CUIWeaponParams::RifleAmmo(CWeapon& weapon)
{
	SAmmo ammo;
	ammo.types = &weapon.m_ammoTypes;
	ammo.capacity = weapon.GetAmmoMagSize();
	u8 type = weapon.AmmoType.MagazineType;
	const xr_vector<CCartridge>* next = &weapon.m_magazine;
	if (weapon.IsChamber() && !weapon.m_chamber.empty())
		next = &weapon.m_chamber;

	// The type picked for the next reload (in launcher mode it is the grenades').
	if (weapon.m_set_next_ammoType_on_reload != CWeapon::undefined_ammo_type)
		type = weapon.m_set_next_ammoType_on_reload;

	// Left in launcher mode: PerformSwitchGL swapped the rifle's ammo out.
	if (CWeaponMagazinedWGrenade* launcher = smart_cast<CWeaponMagazinedWGrenade*>(&weapon))
	{
		if (launcher->IsGrenadeMode())
		{
			ammo.types = &launcher->m_ammoTypes2;
			ammo.capacity = launcher->iMagazineSize2;
			type = launcher->AmmoType.GrenadeType;
			next = &launcher->m_magazine2;
		}
	}

	// The next shot's cartridge; empty, the type the weapon will load.
	if (!next->empty())
	{
		const CCartridge& cartridge = next->back();
		ammo.section = cartridge.m_ammoSect;
		ammo.shown = cartridge.m_LocalAmmoType;
	}
	else if (type < ammo.types->size())
	{
		ammo.section = (*ammo.types)[type];
		ammo.shown = type;
	}
	return ammo;
}

void CUIWeaponParams::Read(CWeapon& weapon, const SAmmo& ammo, SParams& params)
{
	using namespace WeaponUi;
	LPCSTR sect = ammo.section.size() ? ammo.section.c_str() : nullptr;
	params.k_hit = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "k_hit", 1.0f) : 1.0f;
	params.k_ap = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "k_ap", 0.0f) : 0.0f;
	params.k_disp = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "k_disp", 1.0f) : 1.0f;
	params.impair = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "impair", 1.0f) : 1.0f;
	params.pellets = sect ? READ_IF_EXISTS(pSettings, r_s32, sect, "buck_shot", 1) : 1;
	const auto& silencer = weapon.cur_silencer_koef;
	params.condition = weapon.GetCondition();

	// The player's shot (CShootingObject::FireBullet).
	params.hit_power = weapon.fvHitPower[IsGameTypeSingle() ? g_SingleGameDifficulty : egdMaster];
	params.silencer_hit = silencer.hit_power;

	params.dispersion_deg = weapon.fireDispersionBase * RadToDeg;
	params.silencer_dispersion = silencer.fire_dispersion;
	params.condition_factor = weapon.fireDispersionConditionFactor;

	params.crosshair_inertion = weapon.m_crosshair_inertion;
	params.zoom_rotate_time = weapon.m_zoom_params.m_fZoomRotateTime;
	params.control_inertion = weapon.GetControlInertionFactor();
	params.pdm_base = weapon.Get_PDM_Base();
	params.pdm_velocity = weapon.Get_PDM_Vel_F();

	// CWeaponShotEffector::Shot: a weapon with a recoil pattern kicks by the
	// pattern (no silencer factor), the others by cam_dispersion.
	const xr_vector<CWeapon::SRecoilPoint>& pattern = weapon.m_hipfire_pattern.bullet_patterns;
	params.pattern_step = 0.0f;
	if (!pattern.empty())
	{
		float steps = 0.0f;
		for (const CWeapon::SRecoilPoint& point : pattern)
			steps += _sqrt(point.x * point.x + point.y * point.y);
		params.pattern_step = steps / pattern.size();
	}
	params.pattern_factor = weapon.cam_recoil.Pattern.Factor;
	params.zoom_pattern_factor = weapon.zoom_cam_recoil.Pattern.Factor;
	params.cam[0] = weapon.cam_recoil.Dispersion * silencer.cam_dispersion;
	params.cam[1] = weapon.cam_recoil.DispersionInc * silencer.cam_disper_inc;
	params.cam[2] = weapon.zoom_cam_recoil.Dispersion * silencer.cam_dispersion;
	params.cam[3] = weapon.zoom_cam_recoil.DispersionInc * silencer.cam_disper_inc;

	params.legacy_misfire = weapon.useLegacyMisfire;
	params.misfire_probability = weapon.misfireProbability;
	params.misfire_condition_k = weapon.misfireConditionK;
	params.misfire_start_condition = weapon.misfireStartCondition;
	params.misfire_end_condition = weapon.misfireEndCondition;
	params.misfire_start_prob = weapon.misfireStartProbability;
	params.misfire_end_prob = weapon.misfireEndProbability;

	params.shot_dec = weapon.conditionDecreasePerShot;
}

void CUIWeaponParams::Score(const SParams& params, SScore (&scores)[eBarCount], float& dispersion)
{
	using namespace WeaponUi;
	scores[eBarDamage].value = DamageScore(Damage(params.hit_power, params.k_hit, params.silencer_hit, params.pellets));
	scores[eBarPenetration].max = params.k_ap >= PenetrationMax;
	scores[eBarPenetration].value = scores[eBarPenetration].max ? 0.0f : PenetrationScore(params.k_ap);

	dispersion = Dispersion(params.dispersion_deg, params.k_disp, params.silencer_dispersion,
		params.condition_factor, params.condition);
	scores[eBarAccuracy].value = AccuracyScore(dispersion);

	// The player's actor (CActor::Load reads disp_base in degrees).
	const float hip = HipDispersion(READ_IF_EXISTS(pSettings, r_float, "actor", "disp_base", 0.0f), params.pdm_base,
		READ_IF_EXISTS(pSettings, r_float, "actor", "disp_vel_factor", 0.0f), params.pdm_velocity);
	scores[eBarHandling].value = Handling(params.crosshair_inertion, params.zoom_rotate_time,
		params.control_inertion, hip);

	const float recoil = params.pattern_step > 0.0f
		? PatternRecoil(params.pattern_step, params.pattern_factor, params.zoom_pattern_factor)
		: LegacyRecoil(params.cam[0], params.cam[1], params.cam[2], params.cam[3]);
	scores[eBarRecoil].value = RecoilScore(recoil);

	scores[eBarHold].value = HoldScore(params.condition_factor);

	const float misfire = params.legacy_misfire
		? LegacyMisfireMean(params.misfire_probability, params.misfire_condition_k, params.condition)
		: MisfireMean(params.misfire_start_condition, params.misfire_end_condition,
			params.misfire_start_prob, params.misfire_end_prob, params.condition);
	scores[eBarReliability].max = misfire <= 0.0f;
	scores[eBarReliability].value = scores[eBarReliability].max ? 0.0f : ReliabilityScore(misfire);

	// Wear per shot is condition_shot_dec x the cartridge's impair (CWeapon::FireTrace).
	scores[eBarDurability].max = params.shot_dec * params.impair <= 0.0f;
	scores[eBarDurability].value = scores[eBarDurability].max ? 0.0f
		: DurabilityScore(ShotsPerLife(params.shot_dec, params.impair));
}

// The figures of the text rows as SetInfo prints them (the rounding is the
// printed one, so the differences are exact for the reader).
void CUIWeaponParams::RowValues(CWeapon& weapon, const SAmmo& ammo, float dispersion, SRowValues& values)
{
	values.capacity = ammo.capacity;
	// A double-barrel's rpm is only the gap between its two barrels, not a rate of fire.
	values.rpm = dynamic_cast<CWeaponBM16*>(&weapon) ? -1 : iFloor(60.0f / weapon.getRPM() + 0.5f);
	values.speed = iFloor(weapon.getStartBulletSpeed() * weapon.cur_silencer_koef.bullet_speed + 0.5f);
	values.dispersion = dispersion;
	const float range = MeasureRange(weapon, ammo, dispersion);
	values.range = range >= 0.0f ? iFloor(range + 0.5f) : -1;
}

void CUIWeaponParams::Measure(CWeapon& weapon, const SAmmo& ammo, SScore (&scores)[eBarCount], float& dispersion)
{
	SParams params;
	Read(weapon, ammo, params);
	Score(params, scores, dispersion);
}

// A number an upgrade adds (CInventoryItem::process_if_exists: present and not empty).
static float UpgradeValue(LPCSTR section, LPCSTR key)
{
	if (!pSettings->line_exist(section, key))
		return 0.0f;
	LPCSTR value = pSettings->r_string(section, key);
	return value && *value ? pSettings->r_float(section, key) : 0.0f;
}

// The keys the bars read, added like CWeapon::install_upgrade_disp and
// CInventoryItem::install_upgrade_impl (cam_* in degrees); sign -1 takes an
// installed section off.
void CUIWeaponParams::AddUpgrade(CWeapon& weapon, LPCSTR section, float sign, SParams& params)
{
	const auto& silencer = weapon.cur_silencer_koef;
	auto add = [&](LPCSTR key, float& value, float scale = 1.0f)
	{
		value += sign * scale * UpgradeValue(section, key);
	};
	// A relative key (CInventoryItem::process_if_exists_mul) scales after the
	// section's additions, so taking the section off divides before they are undone.
	auto mul = [&](LPCSTR key, LPCSTR added, float& value)
	{
		const float factor = 1.0f + UpgradeValue(section, key);
		if (sign > 0.0f)
		{
			add(added, value);
			value *= factor;
		}
		else
		{
			if (factor > 0.0f)
				value /= factor;
			add(added, value);
		}
	};
	add("fire_dispersion_base", params.dispersion_deg);
	add("fire_dispersion_condition_factor", params.condition_factor);
	add("crosshair_inertion", params.crosshair_inertion);
	add("zoom_rotate_time", params.zoom_rotate_time);
	add("control_inertion_factor", params.control_inertion);
	add("PDM_disp_base", params.pdm_base);
	add("PDM_disp_vel_factor", params.pdm_velocity);
	mul("pattern_factor_mul", "pattern_factor", params.pattern_factor);
	mul("zoom_pattern_factor_mul", "zoom_pattern_factor", params.zoom_pattern_factor);
	add("cam_dispersion", params.cam[0], deg2rad(1.0f) * silencer.cam_dispersion);
	add("cam_dispersion_inc", params.cam[1], deg2rad(1.0f) * silencer.cam_disper_inc);
	add("zoom_cam_dispersion", params.cam[2], deg2rad(1.0f) * silencer.cam_dispersion);
	add("zoom_cam_dispersion_inc", params.cam[3], deg2rad(1.0f) * silencer.cam_disper_inc);
	add("misfire_probability", params.misfire_probability);
	add("misfire_condition_k", params.misfire_condition_k);
	add("misfire_start_condition", params.misfire_start_condition);
	add("misfire_end_condition", params.misfire_end_condition);
	add("misfire_start_prob", params.misfire_start_prob);
	add("misfire_end_prob", params.misfire_end_prob);
	add("condition_shot_dec", params.shot_dec);

	// The engine's clamps (weapon-upgrade-aim-params). Taking a section off
	// cannot undo one: a clamped installed value counts as it is.
	params.crosshair_inertion = std::max(params.crosshair_inertion, 0.0f);
	params.zoom_rotate_time = std::max(params.zoom_rotate_time, EPS_L);
}

void CUIWeaponParams::MeasureUpgrades(CWeapon& weapon, const xr_vector<LPCSTR>& sections, bool installed,
	SUpgradeRow (&rows)[eUpgradeRowCount])
{
	const float sign = installed ? -1.0f : 1.0f;
	const SAmmo now_ammo = RifleAmmo(weapon);
	SAmmo other_ammo = now_ammo;
	float rpm = 0.0f, speed = 0.0f, weight = 0.0f, capacity = 0.0f, impulse = 0.0f;
	for (LPCSTR section : sections)
	{
		// A calibre change sets the ammo types and loads the first one; taken
		// off, the weapon's own first type is back.
		if (pSettings->line_exist(section, "ammo_class"))
		{
			LPCSTR types = installed ? READ_IF_EXISTS(pSettings, r_string, weapon.cNameSect(), "ammo_class", "")
				: pSettings->r_string(section, "ammo_class");
			string128 first;
			other_ammo.section = _GetItem(types, 0, first);
		}
		rpm += UpgradeValue(section, "rpm");
		speed += UpgradeValue(section, "bullet_speed");
		weight += UpgradeValue(section, "inv_weight");
		capacity += UpgradeValue(section, "ammo_mag_size");
		impulse += UpgradeValue(section, "hit_impulse");
	}

	SParams now, other;
	Read(weapon, now_ammo, now);
	Read(weapon, other_ammo, other);
	for (LPCSTR section : sections)
		AddUpgrade(weapon, section, sign, other);

	SScore now_scores[eBarCount], other_scores[eBarCount];
	float dispersion;
	Score(now, now_scores, dispersion);
	Score(other, other_scores, dispersion);
	const SScore* before = installed ? other_scores : now_scores;
	const SScore* after = installed ? now_scores : other_scores;

	static const struct { EUpgradeRow row; EBar bar; } bars[] = {
		{ eUpgradeAccuracy, eBarAccuracy }, { eUpgradeHandling, eBarHandling },
		{ eUpgradeRecoil, eBarRecoil }, { eUpgradeHold, eBarHold },
		{ eUpgradeReliability, eBarReliability }, { eUpgradeDurability, eBarDurability } };
	for (const auto& bar : bars)
	{
		SUpgradeRow& row = rows[bar.row];
		row.delta = after[bar.bar].value - before[bar.bar].value;
		row.after = after[bar.bar].value;
		row.before_max = before[bar.bar].max;
		row.after_max = after[bar.bar].max;
	}

	// The rows' units: rpm is added as shots per minute, the speed is the
	// tooltip's (with the silencer), the impulse a share of the weapon's.
	rows[eUpgradeRpm] = { rpm, 0.0f, false, false };
	rows[eUpgradeSpeed] = { speed * weapon.cur_silencer_koef.bullet_speed, 0.0f, false, false };
	rows[eUpgradeWeight] = { weight, 0.0f, false, false };
	rows[eUpgradeCapacity] = { capacity, 0.0f, false, false };
	const float base_impulse = weapon.getHitImpulse() - (installed ? impulse : 0.0f);
	rows[eUpgradeImpulse] = { base_impulse > 0.0f ? 100.0f * impulse / base_impulse : 0.0f, 0.0f, false, false };
}

// Effective range (model V4, docs/features/ui-weapon-params.md): the 50% hit
// distance from the weapon's dispersion and the practical aiming dispersion of
// its platform and sight, capped by the bullet's flight and the shot's reach.
// The weapon names its platform (ui_range_platform), [ui_weapon_range] holds
// "<degrees>, <iron sights>" per platform, the sights and the field of view
// limits of the scope classes. Negative: no platform, no row.
float CUIWeaponParams::MeasureRange(CWeapon& weapon, const SAmmo& ammo, float dispersion)
{
	using namespace WeaponUi;
	LPCSTR table = "ui_weapon_range";
	LPCSTR platform = READ_IF_EXISTS(pSettings, r_string, weapon.cNameSect(), "ui_range_platform", nullptr);
	// Multiplayer bullets lose speed by their cartridge, not the global resistance.
	if (!platform || !IsGameTypeSingle() || !pSettings->section_exist(table))
		return -1.0f;
	string64 key;
	xr_strconcat(key, "platform_", platform);
	if (!pSettings->line_exist(table, key))
		return -1.0f;
	LPCSTR line = pSettings->r_string(table, key);
	string64 item;
	const float platform_disp = float(atof(_GetItem(line, 0, item)));

	// A scope (attached or built in) by its zoom, otherwise the platform's iron sights.
	// The scope's own field of view: CurrentZoomFactor gives the launcher's sights in
	// grenade mode, and the range is the rifle's.
	string64 sight;
	if (weapon.IsScopeAttached())
		xr_strcpy(sight, ScopeSight(weapon.m_zoom_params.m_fScopeZoomFactor, pSettings->r_float(table, "red_dot_min_fov"),
			pSettings->r_float(table, "optic_high_max_fov")));
	else
		xr_strconcat(sight, "sight_", _GetItem(line, 1, item));
	if (!pSettings->line_exist(table, sight))
		return -1.0f;
	const float aiming = AimingDispersion(platform_disp, pSettings->r_float(table, sight));

	// The player's bullet (CShootingObject::FireBullet, CBulletManager in single player).
	LPCSTR sect = ammo.section.size() ? ammo.section.c_str() : nullptr;
	const float k_dist = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "k_dist", 1.0f) : 1.0f;
	const float speed = weapon.getStartBulletSpeed() * weapon.cur_silencer_koef.bullet_speed;
	return EffectiveRange(AccuracyRange(dispersion, aiming),
		BallisticRange(speed, pSettings->r_float("bullet_manager", "air_resistance_k"),
			pSettings->r_float("bullet_manager", "gravity_const")),
		weapon.getFireDistance() * k_dist);
}

void CUIWeaponParams::Place(CUIWindow* wnd, float& h)
{
	Fvector2 pos = wnd->GetWndPos();
	pos.y = h;
	wnd->SetWndPos(pos);
	h += wnd->GetWndSize().y;
	AttachChild(wnd);
}

void CUIWeaponParams::PlaceSeparator(u32 index, float& h)
{
	h += m_gap;
	if (m_separators[index])
		Place(m_separators[index], h);
}

// The difference as "+4" / "-3" in the column behind the number (bars and rows
// alike), coloured by whether the weapon is better. No difference: nothing.
void CUIWeaponParams::SetDelta(CUIStatic* wnd, const SDelta& delta)
{
	if (!wnd)
		return;
	if (delta.steps == 0)
	{
		wnd->SetText("");
		return;
	}
	string32 number;
	float scale = 1.0f;
	for (int i = 0; i < delta.decimals; ++i)
		scale *= 10.0f;
	ConditionUi::FormatNumber(number, delta.steps / scale, ConditionUi::DecimalSeparator(), true, delta.decimals);
	wnd->SetText(number);
	wnd->SetTextColor((delta.steps > 0) == delta.higher_better ? m_delta_better_color : m_delta_worse_color);
}

void CUIWeaponParams::SetRow(ERow index, LPCSTR text, float& h, const SDelta& delta)
{
	SRow& row = m_rows[index];
	row.value->SetText(text);
	SetDelta(row.delta, delta);
	Place(row.wnd, h);
}

// The bar is the share of the full bar; past it the bar stays full and the
// triangle shows. The number is the score, "max" when there is no limit.
void CUIWeaponParams::SetBar(EBar index, const SScore& cur, const SScore& slot, bool compared, float& h)
{
	SBar& bar = m_bars[index];
	if (!bar.wnd)
		return;
	auto fill = [&bar](const SScore& score)
	{
		return score.max ? 100.0f : WeaponUi::Clamp100(score.value * 100.0f / bar.full);
	};
	bar.bar->SetTwoPos(fill(cur), fill(slot));
	const bool overflow = cur.max || cur.value > bar.full;
	if (bar.overflow)
		bar.overflow->Show(overflow);
	if (bar.overflow_fill)
		bar.overflow_fill->Show(overflow);
	if (cur.max)
		bar.value->SetTextST("ui_uip_wpn_max");
	else
	{
		string32 text;
		xr_sprintf(text, "%d", cur.value > 0.0f ? iFloor(cur.value + 0.5f) : 0);
		bar.value->SetText(text);
	}
	// The difference of the two printed numbers (every bar: higher is better); none
	// against nothing or when either has no number.
	SDelta delta;
	if (compared && !cur.max && !slot.max)
		delta.steps = WeaponUi::ShownDelta(cur.value, slot.value, 0);
	SetDelta(bar.delta, delta);
	Place(bar.wnd, h);
}

// The condition shown like the slot's bar (GetConditionToShow), coloured by its
// min/middle/max colours, with no comparison: two weapons' wear is not a merit.
void CUIWeaponParams::SetCondition(float condition, float& h)
{
	m_condition.bar->SetProgressPos(condition * 100.0f);
	string32 text;
	xr_sprintf(text, "%d%%", iFloor(condition * 100.0f + 0.5f));
	m_condition.value->SetText(text);
	Place(m_condition.wnd, h);
}

// One row of the rifle's ammo icons at their inventory size, wrapped at the
// tooltip's width; the type the numbers are for at full colour, the rest dimmed.
void CUIWeaponParams::SetAmmoIcons(const SAmmo& ammo, float& h)
{
	if (fis_zero(m_icons_cell) || !ammo.types || ammo.types->empty())
		return;

	h += m_gap;
	float x = m_icons_x;
	float row_y = h;
	float row_height = 0.0f;
	for (u32 i = 0; i < ammo.types->size(); ++i)
	{
		LPCSTR sect = (*ammo.types)[i].c_str();
		if (i >= m_ammo_icons.size())
		{
			CUIStatic* icon = new CUIStatic();
			icon->SetAutoDelete(false);
			m_ammo_icons.push_back(icon);
		}
		CUIStatic* icon = m_ammo_icons[i];

		const float scale = READ_IF_EXISTS(pSettings, r_float, sect, "inv_scale", 1.0f);
		Frect tex_rect = {};
		tex_rect.x1 = pSettings->r_float(sect, "inv_grid_x") * INV_GRID_WIDTH(scale);
		tex_rect.y1 = pSettings->r_float(sect, "inv_grid_y") * INV_GRID_HEIGHT(scale);
		tex_rect.x2 = pSettings->r_float(sect, "inv_grid_width") * INV_GRID_WIDTH(scale);
		tex_rect.y2 = pSettings->r_float(sect, "inv_grid_height") * INV_GRID_HEIGHT(scale);
		tex_rect.rb.add(tex_rect.lt);
		icon->SetShader(InventoryUtilities::GetEquipmentIconsShader(READ_IF_EXISTS(pSettings, r_string, sect, "icons_texture", nullptr)));
		icon->SetTextureRect(tex_rect);
		icon->TextureOn();
		icon->SetStretchTexture(true);

		const float height = m_icons_cell * pSettings->r_float(sect, "inv_grid_height");
		const float width = height * tex_rect.width() / tex_rect.height() * UI().get_current_kx();
		if (x > m_icons_x && x + width > GetWidth())
		{
			x = m_icons_x;
			row_y += row_height + m_icons_gap;
			row_height = 0.0f;
		}
		icon->SetWndPos(Fvector2().set(x, row_y));
		icon->SetWndSize(Fvector2().set(width, height));
		row_height = std::max(row_height, height);
		const bool lit = int(i) == ammo.shown;
		icon->SetTextureColor(lit ? color_rgba(255, 255, 255, 255) : m_icon_dim_color);
		AttachChild(icon);
		x += width + m_icons_gap;
	}
	h = row_y + row_height;
}

void CUIWeaponParams::SetInfo(CInventoryItem* slot_item, CInventoryItem& item)
{
	DetachAll();
	float h = 0.0f;
	if (m_Prop_line)
	{
		AttachChild(m_Prop_line);
		h = m_Prop_line->GetWndPos().y + m_Prop_line->GetWndSize().y;
	}

	CWeapon& weapon = *item.cast_weapon();
	const SAmmo ammo = RifleAmmo(weapon);
	// Ctrl held: the weapon and the compared one as new, condition 100% (the
	// tooltip is built every frame while hovered, so it follows the key); no Ctrl
	// on a gamepad.
	const bool as_new = !pInput->GetControllerMode() &&
		(pInput->iGetAsyncKeyState(SDL_SCANCODE_LCTRL) || pInput->iGetAsyncKeyState(SDL_SCANCODE_RCTRL));
	const float condition = as_new ? 1.0f : item.GetConditionToShow();
	auto measure = [as_new](CWeapon& measured, const SAmmo& measured_ammo, SScore (&scores)[eBarCount], float& disp)
	{
		SParams params;
		Read(measured, measured_ammo, params);
		if (as_new)
			params.condition = 1.0f;
		Score(params, scores, disp);
	};
	SScore cur[eBarCount];
	float dispersion = 0.0f;
	measure(weapon, ammo, cur, dispersion);

	SRowValues cur_values;
	RowValues(weapon, ammo, dispersion, cur_values);

	SScore slot[eBarCount];
	for (u32 i = 0; i < eBarCount; ++i)
		slot[i] = cur[i];
	// The comparison (the line, the second bar, the differences) only while the left Alt is
	// held (the tooltip is built every frame while hovered);
	// no Alt on a gamepad, so no comparison there (ui-weapon-compare-numbers).
	const bool compare_key = !pInput->GetControllerMode() &&
		pInput->iGetAsyncKeyState(SDL_SCANCODE_LALT);
	// The weapon in the compared slot, if it is another one.
	const bool compared = compare_key && slot_item && slot_item != &item && Check(*slot_item);
	SRowValues slot_values = cur_values;
	if (compared)
	{
		CWeapon& other = *slot_item->cast_weapon();
		const SAmmo other_ammo = RifleAmmo(other);
		float other_dispersion = 0.0f;
		measure(other, other_ammo, slot, other_dispersion);
		RowValues(other, other_ammo, other_dispersion, slot_values);
	}

	string32 number;
	string64 text;
	const char separator = ConditionUi::DecimalSeparator();

	if (ammo.section.size())
		SetRow(eRowAmmo, g_pStringTable->translate(pSettings->r_string(ammo.section, "inv_name_short")).c_str(), h);

	// Tube and break-action guns load round by round: no magazine.
	const bool no_magazine = smart_cast<CWeaponShotgun*>(&weapon) || dynamic_cast<CWeaponAutomaticShotgun*>(&weapon);
	m_rows[eRowCapacity].caption->SetTextST(no_magazine ? "ui_uip_wpn_capacity" : "ui_uip_wpn_capacity_mag");
	xr_sprintf(text, "%d %s", ammo.capacity, g_pStringTable->translate("ui_uip_wpn_unit_rounds").c_str());
	SDelta delta;
	delta.steps = compared ? cur_values.capacity - slot_values.capacity : 0;
	SetRow(eRowCapacity, text, h, delta);

	// A double-barrel's rpm is only the gap between its two barrels, not a rate of fire.
	if (!dynamic_cast<CWeaponBM16*>(&weapon))
	{
		xr_sprintf(text, "%d %s", cur_values.rpm, g_pStringTable->translate("ui_uip_wpn_unit_rpm").c_str());
		delta.steps = compared && slot_values.rpm >= 0 ? cur_values.rpm - slot_values.rpm : 0;
		SetRow(eRowRpm, text, h, delta);
	}

	xr_sprintf(text, "%d %s", cur_values.speed, g_pStringTable->translate("ui_uip_wpn_unit_speed").c_str());
	delta.steps = compared ? cur_values.speed - slot_values.speed : 0;
	SetRow(eRowSpeed, text, h, delta);

	// The spread: less is better.
	ConditionUi::FormatNumber(number, dispersion, separator, false, 3);
	xr_sprintf(text, "%s%s", number, g_pStringTable->translate("ui_uip_wpn_unit_deg").c_str());
	SDelta spread;
	spread.decimals = 3;
	spread.higher_better = false;
	spread.steps = compared ? WeaponUi::ShownDelta(cur_values.dispersion, slot_values.dispersion, 3) : 0;
	SetRow(eRowDispersion, text, h, spread);

	if (cur_values.range >= 0)
	{
		xr_sprintf(text, "%d %s", cur_values.range, g_pStringTable->translate("ui_uip_wpn_unit_m").c_str());
		delta.steps = compared && slot_values.range >= 0 ? cur_values.range - slot_values.range : 0;
		SetRow(eRowRange, text, h, delta);
	}

	if (!m_condition.wnd)
	{
		xr_sprintf(text, "%d%%", iFloor(condition * 100.0f + 0.5f));
		SetRow(eRowCondition, text, h);
	}

	PlaceSeparator(0, h);
	// What the bars compare with: the weapon in the compared slot, none, or this one
	// (the actor menu picks the slot, ui-weapon-compare).
	if (m_compare && compare_key)
	{
		const bool other = compared;
		const shared_str none = g_pStringTable->translate(slot_item == &item ? "ui_uip_wpn_compare_self" : "ui_uip_wpn_compare_none");
		LPCSTR name = other ? slot_item->NameShort() : none.c_str();
		string256 compare;
		xr_strconcat(compare, g_pStringTable->translate("ui_uip_wpn_compare").c_str(), ": ", name);
		m_compare->SetText(compare);
		Place(m_compare, h);
	}
	for (u32 i = 0; i < eFirstWearBar; ++i)
		SetBar(EBar(i), cur[i], slot[i], compared, h);

	PlaceSeparator(1, h);
	if (m_condition.wnd)
		SetCondition(condition, h);
	for (u32 i = eFirstWearBar; i < eBarCount; ++i)
		SetBar(EBar(i), cur[i], slot[i], compared, h);

	PlaceSeparator(2, h);
	SetAmmoIcons(ammo, h);

	SetHeight(h);
}
