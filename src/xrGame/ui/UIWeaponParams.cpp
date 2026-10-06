#include "stdafx.h"
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
#include "../../xrEngine/string_table.h"

static LPCSTR weapon_bar_node[] =
{
	"damage",
	"accuracy",
	"handling",
	"recoil",
	"reliability",
	"durability",
};

static LPCSTR weapon_bar_caption[] =
{
	"ui_uip_wpn_damage",
	"ui_uip_wpn_accuracy",
	"ui_uip_wpn_handling",
	"ui_uip_wpn_recoil",
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
	"ui_uip_wpn_condition",
};

CUIWeaponParams::CUIWeaponParams()
{
}

CUIWeaponParams::~CUIWeaponParams()
{
	xr_delete(m_Prop_line);
	for (CUIStatic*& separator : m_separators)
		xr_delete(separator);
	for (u32 i = 0; i < eBarCount; ++i)
		xr_delete(m_bars[i].wnd);
	for (u32 i = 0; i < eRowCount; ++i)
		xr_delete(m_rows[i].wnd);
	for (CUIStatic*& icon : m_ammo_icons)
		xr_delete(icon);
}

// <weapon_params gap=""> holds a <row> template (caption, value), one node per
// bar (<damage>, <accuracy>, <handling>, <recoil>, <reliability>, <durability>:
// optional icon, caption, progress as a double bar with color_less/color_more,
// overflow, overflow_fill, value), an optional <separator> after the gap between the
// groups and <ammo_icons x="" cell="" gap=""> (cell: the height of one inventory
// grid cell) with a <dim> colour for the ammo the numbers are not for.
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
		row.caption = UIHelper::CreateTextWnd(xml, "row:caption", row.wnd);
		row.caption->SetTextST(weapon_row_caption[i]);
		row.value = UIHelper::CreateTextWnd(xml, "row:value", row.wnd);
	}

	string256 buf;
	for (u32 i = 0; i < eBarCount; ++i)
	{
		SBar& bar = m_bars[i];
		LPCSTR node = weapon_bar_node[i];
		bar.wnd = new CUIWindow();
		bar.wnd->SetAutoDelete(false);
		CUIXmlInit::InitWindow(xml, node, 0, bar.wnd);
		xr_strconcat(buf, node, ":icon");
		if (xml.NavigateToNode(buf))
			UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":caption");
		UIHelper::CreateTextWnd(xml, buf, bar.wnd)->SetTextST(weapon_bar_caption[i]);
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
		bar.value = UIHelper::CreateTextWnd(xml, buf, bar.wnd);
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
	u8 type = weapon.m_ammoType;
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
			type = launcher->m_ammoType2;
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

void CUIWeaponParams::Measure(CWeapon& weapon, const SAmmo& ammo, SScore (&scores)[eBarCount], float& dispersion)
{
	using namespace WeaponUi;
	LPCSTR sect = ammo.section.size() ? ammo.section.c_str() : nullptr;
	const float k_hit = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "k_hit", 1.0f) : 1.0f;
	const float k_disp = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "k_disp", 1.0f) : 1.0f;
	const float impair = sect ? READ_IF_EXISTS(pSettings, r_float, sect, "impair", 1.0f) : 1.0f;
	const int pellets = sect ? READ_IF_EXISTS(pSettings, r_s32, sect, "buck_shot", 1) : 1;
	const auto& silencer = weapon.cur_silencer_koef;
	const float condition = weapon.GetCondition();

	// The player's shot (CShootingObject::FireBullet).
	const float hit_power = weapon.fvHitPower[IsGameTypeSingle() ? g_SingleGameDifficulty : egdMaster];
	scores[eBarDamage].value = DamageScore(Damage(hit_power, k_hit, silencer.hit_power, pellets));

	dispersion = Dispersion(weapon.fireDispersionBase * RadToDeg, k_disp, silencer.fire_dispersion,
		weapon.fireDispersionConditionFactor, condition);
	scores[eBarAccuracy].value = AccuracyScore(dispersion);

	scores[eBarHandling].value = Handling(weapon.m_crosshair_inertion, weapon.m_zoom_params.m_fZoomRotateTime,
		weapon.GetControlInertionFactor());

	// CWeaponShotEffector::Shot: a weapon with a recoil pattern kicks by the
	// pattern (no silencer factor), the others by cam_dispersion.
	const xr_vector<CWeapon::SRecoilPoint>& pattern = weapon.m_hipfire_pattern.bullet_patterns;
	float recoil;
	if (!pattern.empty())
	{
		float steps = 0.0f;
		for (const CWeapon::SRecoilPoint& point : pattern)
			steps += _sqrt(point.x * point.x + point.y * point.y);
		recoil = PatternRecoil(steps / pattern.size(), weapon.cam_recoil.Pattern.Factor,
			weapon.zoom_cam_recoil.Pattern.Factor);
	}
	else
	{
		recoil = LegacyRecoil(weapon.cam_recoil.Dispersion * silencer.cam_dispersion,
			weapon.cam_recoil.DispersionInc * silencer.cam_disper_inc,
			weapon.zoom_cam_recoil.Dispersion * silencer.cam_dispersion,
			weapon.zoom_cam_recoil.DispersionInc * silencer.cam_disper_inc);
	}
	scores[eBarRecoil].value = RecoilScore(recoil);

	const float misfire = weapon.useLegacyMisfire
		? LegacyMisfireMean(weapon.misfireProbability, weapon.misfireConditionK, condition)
		: MisfireMean(weapon.misfireStartCondition, weapon.misfireEndCondition,
			weapon.misfireStartProbability, weapon.misfireEndProbability, condition);
	scores[eBarReliability].max = misfire <= 0.0f;
	scores[eBarReliability].value = scores[eBarReliability].max ? 0.0f : ReliabilityScore(misfire);

	// Wear per shot is condition_shot_dec x the cartridge's impair (CWeapon::FireTrace).
	scores[eBarDurability].max = weapon.conditionDecreasePerShot * impair <= 0.0f;
	scores[eBarDurability].value = scores[eBarDurability].max ? 0.0f
		: DurabilityScore(ShotsPerLife(weapon.conditionDecreasePerShot, impair));
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

void CUIWeaponParams::SetRow(ERow index, LPCSTR text, float& h)
{
	m_rows[index].value->SetText(text);
	Place(m_rows[index].wnd, h);
}

// The bar is the share of the full bar; past it the bar stays full and the
// triangle shows. The number is the score, "max" when there is no limit.
void CUIWeaponParams::SetBar(EBar index, const SScore& cur, const SScore& slot, float& h)
{
	SBar& bar = m_bars[index];
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
	Place(bar.wnd, h);
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
		row_height = _max(row_height, height);
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
	SScore cur[eBarCount];
	float dispersion = 0.0f;
	Measure(weapon, ammo, cur, dispersion);

	SScore slot[eBarCount];
	for (u32 i = 0; i < eBarCount; ++i)
		slot[i] = cur[i];
	if (slot_item && slot_item != &item && Check(*slot_item))
	{
		CWeapon& other = *slot_item->cast_weapon();
		float other_dispersion = 0.0f;
		Measure(other, RifleAmmo(other), slot, other_dispersion);
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
	SetRow(eRowCapacity, text, h);

	// A double-barrel's rpm is only the gap between its two barrels, not a rate of fire.
	if (!dynamic_cast<CWeaponBM16*>(&weapon))
	{
		xr_sprintf(text, "%d %s", iFloor(60.0f / weapon.getRPM() + 0.5f), g_pStringTable->translate("ui_uip_wpn_unit_rpm").c_str());
		SetRow(eRowRpm, text, h);
	}

	xr_sprintf(text, "%d %s", iFloor(weapon.getStartBulletSpeed() * weapon.cur_silencer_koef.bullet_speed + 0.5f),
		g_pStringTable->translate("ui_uip_wpn_unit_speed").c_str());
	SetRow(eRowSpeed, text, h);

	ConditionUi::FormatNumber(number, dispersion, separator, false, 3);
	xr_sprintf(text, "%s%s", number, g_pStringTable->translate("ui_uip_wpn_unit_deg").c_str());
	SetRow(eRowDispersion, text, h);

	xr_sprintf(text, "%d%%", iFloor(item.GetConditionToShow() * 100.0f + 0.5f));
	SetRow(eRowCondition, text, h);

	PlaceSeparator(0, h);
	for (u32 i = 0; i < eBarCount; ++i)
		SetBar(EBar(i), cur[i], slot[i], h);

	PlaceSeparator(1, h);
	SetAmmoIcons(ammo, h);

	SetHeight(h);
}
