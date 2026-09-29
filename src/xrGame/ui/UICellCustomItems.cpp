#include "stdafx.h"
#include "UICellCustomItems.h"
#include "UIInventoryUtilities.h"
#include "../Weapon.h"
#include "UIDragDropListEx.h"
#include "../../xrUI/Widgets/UIProgressBar.h"

namespace detail 
{

struct is_helper_pred
{
	bool operator ()(CUICellItem* child)
	{
		return child->IsHelper();
	}

}; // struct is_helper_pred

} //namespace detail 

// Optional per-weapon icon of an addon overlay: <prefix>_icons_texture, _grid_x, _grid_y,
// _grid_width, _grid_height and _inv_scale in the weapon's (or scope's) section. Every key
// falls back on its own to the addon's section and the weapon's atlas, so without any of
// them the overlay is the legacy one. _weapon_offset_x/_y move the whole icon.
// Offsets are pixels at scale 1 (50 per cell); with scaled_offsets (the weapon's
// addon_offsets_use_inv_scale) they are pixels of the weapon's own atlas instead, so
// they are divided by its inv_scale.
IC void read_addon_icon_layout(LPCSTR icon_section, LPCSTR prefix, LPCSTR addon_section, bool scaled_offsets, float weapon_scale, SAddonIconLayout& l)
{
	string128 key;
	auto own = [&](LPCSTR suffix)
	{
		xr_sprintf(key, "%s_%s", prefix, suffix);
		return !!pSettings->line_exist(icon_section, key);
	};
	auto cells = [&](LPCSTR suffix, LPCSTR addon_key)
	{
		if (own(suffix))
		{
			l.custom = true;
			return float(pSettings->r_u32(icon_section, key));
		}
		return float(pSettings->r_u32(addon_section, addon_key));
	};

	l.custom = false;
	l.texture = nullptr;
	if (own("icons_texture"))
	{
		l.texture = pSettings->r_string(icon_section, key);
		l.custom = true;
	}

	l.grid_pos.x = cells("grid_x", "inv_grid_x");
	l.grid_pos.y = cells("grid_y", "inv_grid_y");
	l.grid_size.x = cells("grid_width", "inv_grid_width");
	l.grid_size.y = cells("grid_height", "inv_grid_height");

	if (own("inv_scale"))
	{
		l.scale = pSettings->r_float(icon_section, key);
		l.custom = true;
	}
	else
		l.scale = READ_IF_EXISTS(pSettings, r_float, addon_section, "inv_scale", 1.0f);

	const float unit = scaled_offsets ? weapon_scale : 1.0f;
	if (scaled_offsets)
		l.custom = true;

	l.offset.set(0.0f, 0.0f);
	if (l.custom)
	{
		xr_sprintf(key, "%s_x", prefix);
		l.offset.x = float(pSettings->r_s32(icon_section, key)) / unit;
		xr_sprintf(key, "%s_y", prefix);
		l.offset.y = float(pSettings->r_s32(icon_section, key)) / unit;
	}

	l.shift.x = own("weapon_offset_x") ? pSettings->r_float(icon_section, key) / unit : 0.0f;
	l.shift.y = own("weapon_offset_y") ? pSettings->r_float(icon_section, key) / unit : 0.0f;

	l.extra.x = own("weapon_extra_width") ? int(pSettings->r_u32(icon_section, key)) : 0;
	l.extra.y = own("weapon_extra_height") ? int(pSettings->r_u32(icon_section, key)) : 0;
}

// One side of a weapon's cell: its own length plus what the addons add, but never longer
// than the list, which has to hold it (a list with no room left ends in R_ASSERT).
IC int weapon_grid_len(int base, int extra, int capacity)
{
	return _max(base, _min(base + extra, capacity));
}

// The part of a grown cell the weapon's own sprite takes: its size in the top left
// corner, as the addon overlays and offsets are placed from there.
IC Fvector2 weapon_sprite_size(const Fvector2& wnd, const Ivector2& grid, const Ivector2& base)
{
	return Fvector2().set(wnd.x * base.x / grid.x, wnd.y * base.y / grid.y);
}

// Where the weapon's own sprite goes for a shift in logical units (50 per cell). A headed
// cell draws the sprite turned by 90 degrees and squeezed by kx across the screen: the
// weapon's x runs up the screen and its y to the right, the way InitAddon places the
// overlays.
IC Fvector2 weapon_shift_offset(const Fvector2& shift, const Fvector2& wnd, const Ivector2& grid, bool heading, float kx)
{
	Fvector2 res;
	if (heading)
		res.set(shift.y * wnd.x / (INV_GRID_HEIGHT(1.0f) * grid.y) * kx, -shift.x * wnd.y / (INV_GRID_WIDTH(1.0f) * grid.x));
	else
		res.set(shift.x * wnd.x / (INV_GRID_WIDTH(1.0f) * grid.x), shift.y * wnd.y / (INV_GRID_HEIGHT(1.0f) * grid.y));
	return res;
}

// A layer section of a weapon's inv_layers:
// - icons_texture (the weapon's atlas when missing), inv_grid_x/_y/_width/_height
//   (required), inv_scale (the weapon's when on its atlas, else 1);
// - inv_layer_x, inv_layer_y: where it goes on the weapon's sprite, weapon_offset_x/_y:
//   shift of the whole icon, both in the units of the addons' <prefix>_x/_y with icon
//   keys;
// - below_weapon, and the conditions upgrades, no_upgrades (upgrade ids, up_...) and
//   no_addons (silencer, scope, grenade_launcher).
// False with the reason in error when the layer can't be used.
IC bool read_weapon_icon_layer(LPCSTR section, LPCSTR weapon_texture, bool scaled_offsets, float weapon_scale, SWeaponIconLayer& l, string256& error)
{
	if (!pSettings->section_exist(section))
	{
		xr_strcpy(error, "no such section");
		return false;
	}

	static const LPCSTR required[] = { "inv_grid_x", "inv_grid_y", "inv_grid_width", "inv_grid_height" };
	for (LPCSTR key : required)
	{
		if (!pSettings->line_exist(section, key))
		{
			xr_sprintf(error, "no %s", key);
			return false;
		}
	}

	l.no_addons = 0;
	if (pSettings->line_exist(section, "no_addons"))
	{
		static const LPCSTR addons[] = { "silencer", "scope", "grenade_launcher" };	// eAddonType order
		LPCSTR list = pSettings->r_string(section, "no_addons");
		for (int i = 0, count = _GetItemCount(list); i < count; ++i)
		{
			string128 name;
			_GetItem(list, i, name);
			u8 bit = 0;
			for (u8 a = 0; a < 3; ++a)
				if (!xr_strcmp(name, addons[a]))
					bit = u8(1 << a);
			if (!bit)
			{
				xr_sprintf(error, "unknown addon '%s' in no_addons", name);
				return false;
			}
			l.no_addons |= bit;
		}
	}

	auto ids = [&](LPCSTR key, xr_vector<shared_str>& res)
	{
		res.clear();
		if (!pSettings->line_exist(section, key))
			return;
		LPCSTR list = pSettings->r_string(section, key);
		for (int i = 0, count = _GetItemCount(list); i < count; ++i)
		{
			string128 id;
			res.push_back(_GetItem(list, i, id));
		}
	};
	ids("upgrades", l.upgrades);
	ids("no_upgrades", l.no_upgrades);

	l.section = section;
	l.below = !!READ_IF_EXISTS(pSettings, r_bool, section, "below_weapon", false);

	SAddonIconLayout& icon = l.icon;
	const bool own_texture = !!pSettings->line_exist(section, "icons_texture");
	icon.texture = own_texture ? pSettings->r_string(section, "icons_texture") : weapon_texture;
	icon.scale = READ_IF_EXISTS(pSettings, r_float, section, "inv_scale", own_texture ? 1.0f : weapon_scale);
	icon.grid_pos.set(float(pSettings->r_u32(section, "inv_grid_x")), float(pSettings->r_u32(section, "inv_grid_y")));
	icon.grid_size.set(float(pSettings->r_u32(section, "inv_grid_width")), float(pSettings->r_u32(section, "inv_grid_height")));

	const float unit = scaled_offsets ? weapon_scale : 1.0f;
	icon.offset.set(READ_IF_EXISTS(pSettings, r_float, section, "inv_layer_x", 0.0f) / unit,
					READ_IF_EXISTS(pSettings, r_float, section, "inv_layer_y", 0.0f) / unit);
	icon.shift.set(READ_IF_EXISTS(pSettings, r_float, section, "weapon_offset_x", 0.0f) / unit,
				   READ_IF_EXISTS(pSettings, r_float, section, "weapon_offset_y", 0.0f) / unit);
	icon.extra.set(0, 0);
	icon.custom = true;
	return true;
}

// A layer is on while all of its conditions hold: one of upgrades is installed, none of
// no_upgrades is, none of no_addons is on. A layer without conditions is always on.
IC bool weapon_icon_layer_on(const SWeaponIconLayer& l, u8 addons_on, CInventoryItem& item)
{
	if (l.no_addons & addons_on)
		return false;
	for (const shared_str& id : l.no_upgrades)
		if (item.has_upgrade(id))
			return false;
	if (l.upgrades.empty())
		return true;
	for (const shared_str& id : l.upgrades)
		if (item.has_upgrade(id))
			return true;
	return false;
}

// A broken layer is reported once per section, not every time a cell is made.
static void report_weapon_icon_layer(LPCSTR weapon, LPCSTR layer, LPCSTR error)
{
	static xr_set<shared_str> reported;
	if (reported.insert(shared_str(layer)).second)
		Msg("! Inventory icon layer [%s] of [%s] skipped: %s", layer, weapon, error);
}


CUIInventoryCellItem::CUIInventoryCellItem(CInventoryItem* itm)
{
	m_pData = (void*)itm;

	inherited::SetShader(InventoryUtilities::GetEquipmentIconsShader(itm->IconsTexture.c_str()));

	m_grid_size.set(itm->GetInvGridRect().rb);
	Frect rect;
	rect.lt.set(INV_GRID_WIDTH(itm->ScaleIcon) * itm->GetInvGridRect().x1,
		INV_GRID_HEIGHT(itm->ScaleIcon) * itm->GetInvGridRect().y1);

	rect.rb.set(rect.lt.x + INV_GRID_WIDTH(itm->ScaleIcon) * m_grid_size.x,
		rect.lt.y + INV_GRID_HEIGHT(itm->ScaleIcon) * m_grid_size.y);

	inherited::SetTextureRect(rect);
	inherited::SetStretchTexture(true);
}

bool CUIInventoryCellItem::EqualTo(CUICellItem* itm)
{
	CUIInventoryCellItem* ci = smart_cast<CUIInventoryCellItem*>(itm);
	if (!itm)
	{
		return false;
	}

	if (object()->object().cNameSect() != ci->object()->object().cNameSect())
	{
		return false;
	}

	if (!fsimilar(object()->GetCondition(), ci->object()->GetCondition(), 0.01f))
	{
		return false;
	}

	if (!object()->equal_upgrades(ci->object()->upgardes()))
	{
		return false;
	}

	CEatableItem* CiEatableItem = ci->object()->cast_eatable_item();
	CEatableItem* EatableItem = object()->cast_eatable_item();

	if (CiEatableItem != nullptr && EatableItem != nullptr && CiEatableItem->GetMaxUses() > 1 && EatableItem->GetMaxUses() > 1 && CiEatableItem->GetRemainingUses() != EatableItem->GetRemainingUses())
	{
		return false;
	}

	return true;
}

bool CUIInventoryCellItem::IsHelperOrHasHelperChild()
{
	return std::count_if(m_childs.begin(), m_childs.end(), ::detail::is_helper_pred()) > 0 || IsHelper();
}

CUIDragItem* CUIInventoryCellItem::CreateDragItem()
{
	return IsHelperOrHasHelperChild() ? nullptr : inherited::CreateDragItem();
}

bool CUIInventoryCellItem::IsHelper ()
{
	return object()->is_helper_item();
}

void CUIInventoryCellItem::SetIsHelper (bool is_helper)
{
	object()->set_is_helper(is_helper);
}

void CUIInventoryCellItem::Update()
{
	inherited::Update	();
	inherited::UpdateConditionProgressBar(); //Alundaio
	UpdateItemText();

	u32 color = GetTextureColor();
	if ( IsHelper() && !ChildsCount() )
	{
		color = 0xbbbbbbbb;
	}
	else if ( IsHelperOrHasHelperChild() )
	{
		color = 0xffffffff;
	}

	SetTextureColor(color);
}

void CUIInventoryCellItem::UpdateItemText()
{
	const u32	helper_count	=  	(u32)std::count_if(m_childs.begin(), m_childs.end(), ::detail::is_helper_pred()) 
									+ IsHelper() ? 1 : 0;

	const u32	count			=	ChildsCount() + 1 - helper_count;

	string32	str;

	if ( count > 1 || helper_count )
	{
		xr_sprintf						( str, "%d", count );
		m_text->TextItemControl()->SetText	( str );
		m_text->Show					( true );

		// No plate behind the number at any digit count: the bare figure in the
		// corner reads well enough over the icons and does not cover them.
		m_text->TextureOff				();
		PlaceCountText					();
	}
	else
	{
		xr_sprintf						( str, "");
		m_text->TextItemControl()->SetText	( str );
		m_text->Show					( false );
		m_text->TextureOff				();
	}
}

CUIAmmoCellItem::CUIAmmoCellItem(CWeaponAmmo* itm)
:inherited(itm)
{}

bool CUIAmmoCellItem::EqualTo(CUICellItem* itm)
{
	if(!inherited::EqualTo(itm))	return false;

	CUIAmmoCellItem* ci				= smart_cast<CUIAmmoCellItem*>(itm);
	if(!ci)							return false;

	return					( (object()->cNameSect() == ci->object()->cNameSect()) );
}

CUIDragItem* CUIAmmoCellItem::CreateDragItem()
{
	return IsHelper() ? nullptr : inherited::CreateDragItem();
}

u32 CUIAmmoCellItem::CalculateAmmoCount()
{
	u32 total = IsHelper() ? 0 : object()->m_boxCurr;
	for (CUICellItem* child : m_childs)
	{
		if (!child->IsHelper())
		{
			total += ((CUIAmmoCellItem*)(child))->object()->m_boxCurr;
		}
	}

	return total;
}

void CUIAmmoCellItem::UpdateItemText()
{
	m_text->Show( false );
	if ( !m_custom_draw )
	{
		const u32 total = CalculateAmmoCount();
		
		string32			str;
		xr_sprintf			(str, "%d", total);
		m_text->TextItemControl()->SetText(str);
		m_text->Show		(true);
		// Ammo never toggled the label texture, so it kept the fixed plate the XML
		// gives it even for a single digit. Same bare number as every other stack.
		m_text->TextureOff	();
		PlaceCountText		();
	}
}

CUIWeaponCellItem::CUIWeaponCellItem(CWeapon* itm)
:inherited(itm)
{
	m_addons[eSilencer]		= nullptr;
	m_addons[eScope]		= nullptr;
	m_addons[eLauncher]		= nullptr;

	// One switch for the whole weapon, variants inherit it: every addon offset of the
	// icon is in pixels of the weapon's atlas rather than at scale 1.
	m_scaled_offsets		= !!READ_IF_EXISTS(pSettings, r_bool, itm->cNameSect(), "addon_offsets_use_inv_scale", false);
	m_base_grid				= m_grid_size;

	// Cosmetic layers of the icon, read once: variants inherit the list, upgrades only
	// switch the layers on and off.
	LPCSTR layers			= READ_IF_EXISTS(pSettings, r_string, itm->cNameSect(), "inv_layers", nullptr);
	for (int i = 0, count = _GetItemCount(layers); i < count; ++i)
	{
		string128 section;
		string256 error;
		_GetItem(layers, i, section);
		SWeaponIconLayer layer;
		if (m_layers.size() == 32)
			xr_strcpy(error, "more than 32 layers");
		else if (read_weapon_icon_layer(section, itm->IconsTexture.c_str(), m_scaled_offsets, itm->ScaleIcon, layer, error))
		{
			m_layers.push_back(layer);
			continue;
		}
		report_weapon_icon_layer(itm->cNameSect().c_str(), section, error);
	}
	m_layer_icons.resize	(m_layers.size(), nullptr);

	if(itm->SilencerAttachable())
	{
		m_addon_offset[eSilencer].set(object()->GetSilencerX(), object()->GetSilencerY());
	}

	if(itm->ScopeAttachable())
	{
		m_addon_offset[eScope].set(object()->GetScopeX(), object()->GetScopeY());
		mScopeBack = object()->GetScopeBack();
	}

	if(itm->GrenadeLauncherAttachable())
	{
		m_addon_offset[eLauncher].set(object()->GetGrenadeLauncherX(), object()->GetGrenadeLauncherY());
	}
}

#include "../xrServerEntities/object_broker.h"
CUIWeaponCellItem::~CUIWeaponCellItem()
{
}

bool CUIWeaponCellItem::is_scope()
{
	return object()->ScopeAttachable()&&object()->IsScopeAttached();
}

bool CUIWeaponCellItem::is_silencer()
{
	return object()->SilencerAttachable()&&object()->IsSilencerAttached();
}

bool CUIWeaponCellItem::is_launcher()
{
	return object()->GrenadeLauncherAttachable()&&object()->IsGrenadeLauncherAttached();
}

void CUIWeaponCellItem::CreateIcon(eAddonType t)
{
	if(m_addons[t])				return;
	m_addons[t]					= new CUIStatic();	
	m_addons[t]->SetAutoDelete	(true);
	AttachChild					(m_addons[t]);

	const char* sect = nullptr;
	if (t == eSilencer) sect = *object()->GetSilencerName();
	else if (t == eScope) sect = *object()->GetScopeName();
	else if (t == eLauncher) sect = *object()->GetGrenadeLauncherName();

	m_addons[t]->SetShader		(InventoryUtilities::GetEquipmentIconsShader(AddonIconsTexture(t)));

	u32 color = GetTextureColor	();
	m_addons[t]->SetTextureColor(color);
}

void CUIWeaponCellItem::DestroyIcon(eAddonType t)
{
	DetachChild		(m_addons[t]);
	m_addons[t]		= nullptr;
}

CUIStatic* CUIWeaponCellItem::GetIcon(eAddonType t)
{
	return m_addons[t];
}

const SAddonIconLayout& CUIWeaponCellItem::RefreshLayout(eAddonType t)
{
	shared_str icon_section, addon_section;
	LPCSTR prefix;
	switch (t)
	{
	case eSilencer:
		icon_section	= object()->GetSilencerIconSection();
		addon_section	= object()->GetSilencerName();
		prefix			= "silencer";
		break;
	case eScope:
		icon_section	= object()->GetScopeIconSection();
		addon_section	= object()->GetScopeName();
		prefix			= "scope";
		break;
	default:
		icon_section	= object()->GetGrenadeLauncherIconSection();
		addon_section	= object()->GetGrenadeLauncherName();
		prefix			= "grenade_launcher";
		break;
	}

	// Read again only when the scope, an upgrade or the addon changed the sections.
	SAddonIconLayout& l = m_layout[t];
	if (!l.addon_section.size() || l.icon_section != icon_section || l.addon_section != addon_section)
	{
		read_addon_icon_layout(*icon_section, prefix, *addon_section, m_scaled_offsets, object()->ScaleIcon, l);
		l.icon_section	= icon_section;
		l.addon_section	= addon_section;
	}
	return l;
}

LPCSTR CUIWeaponCellItem::AddonIconsTexture(eAddonType t)
{
	const SAddonIconLayout& l = RefreshLayout(t);
	return l.texture.size() ? l.texture.c_str() : object()->IconsTexture.c_str();
}

// Sum of the shifts of the addons that are on: the silencer's and the scope's together
// with all three.
Fvector2 CUIWeaponCellItem::AttachedAddonsShift()
{
	Fvector2 res;
	res.set(0.0f, 0.0f);
	if (is_silencer())
		res.add(RefreshLayout(eSilencer).shift);
	if (is_scope())
		res.add(RefreshLayout(eScope).shift);
	if (is_launcher())
		res.add(RefreshLayout(eLauncher).shift);
	return res;
}

// Cells the addons that are on add to the weapon's cell, summed like the offsets.
Ivector2 CUIWeaponCellItem::AttachedAddonsExtra()
{
	Ivector2 res;
	res.set(0, 0);
	if (is_silencer())
		res.add(RefreshLayout(eSilencer).extra);
	if (is_scope())
		res.add(RefreshLayout(eScope).extra);
	if (is_launcher())
		res.add(RefreshLayout(eLauncher).extra);
	return res;
}

// Base size plus <p>_weapon_extra_width/_height of the addons that are on, never beyond
// the list's columns, nor its rows unless it grows. Vertical lists (the weapon slots)
// keep the base size.
Ivector2 CUIWeaponCellItem::DesiredGridSize(CUIDragDropListEx* list)
{
	Ivector2 size = m_base_grid;
	if (!list || list->GetVerticalPlacement())
		return size;

	const Ivector2 extra		= AttachedAddonsExtra();
	const Ivector2& capacity	= list->CellsCapacity();
	size.x						= weapon_grid_len(m_base_grid.x, extra.x, capacity.x);
	size.y						= list->IsAutoGrow() ? m_base_grid.y + extra.y
									: weapon_grid_len(m_base_grid.y, extra.y, capacity.y);
	return size;
}

// A cell grown for its addons draws the weapon at its own size instead of stretching it.
// The layers not below the weapon go right over its sprite, before the children draw
// the addon overlays on top.
void CUIWeaponCellItem::DrawTexture()
{
	if (Heading() || (m_grid_size.x == m_base_grid.x && m_grid_size.y == m_base_grid.y))
	{
		inherited::DrawTexture();
		DrawLayers(false);
		return;
	}

	const Fvector2 full = GetWndSize();
	SetWndSize(weapon_sprite_size(full, m_grid_size, m_base_grid));
	inherited::DrawTexture();
	SetWndSize(full);
	DrawLayers(false);
}

// Moves only the drawn sprite: the cell, its hit area, count, condition bar and upgrade
// mark stay where the grid put them.
void CUIWeaponCellItem::ApplyWeaponShift()
{
	const Fvector2 offset = weapon_shift_offset(m_shift, GetWndSize(), m_grid_size, Heading(), UI().get_current_kx());
	SetTextureOffset(offset.x, offset.y);
}

// A bit per layer of inv_layers whose conditions hold right now.
u32 CUIWeaponCellItem::ActiveLayers()
{
	u8 addons_on = 0;
	if (is_silencer())
		addons_on |= 1 << eSilencer;
	if (is_scope())
		addons_on |= 1 << eScope;
	if (is_launcher())
		addons_on |= 1 << eLauncher;

	u32 res = 0;
	for (u32 i = 0; i < m_layers.size(); ++i)
		if (weapon_icon_layer_on(m_layers[i], addons_on, *object()))
			res |= 1u << i;
	return res;
}

// Sum of the weapon_offset_x/_y of the layers that are on, added to the addons' one.
Fvector2 CUIWeaponCellItem::LayersShift(u32 layers_on)
{
	Fvector2 res;
	res.set(0.0f, 0.0f);
	for (u32 i = 0; i < m_layers.size(); ++i)
		if (layers_on & (1u << i))
			res.add(m_layers[i].icon.shift);
	return res;
}

// A static for every layer that is on, placed the way the addon overlays are. They are
// children for their position and color only: Draw and DrawTexture draw them in order.
void CUIWeaponCellItem::InitLayers(bool b_rotate)
{
	for (u32 i = 0; i < m_layers.size(); ++i)
	{
		CUIStatic*& s = m_layer_icons[i];
		if (!(m_layers_on & (1u << i)))
		{
			if (s)
			{
				DetachChild(s);
				s = nullptr;
			}
			continue;
		}

		if (!s)
		{
			s = new CUIStatic();
			s->SetAutoDelete(true);
			s->SetCustomDraw(true);
			AttachChild(s);
			s->SetShader(InventoryUtilities::GetEquipmentIconsShader(m_layers[i].icon.texture.c_str()));
			s->SetTextureColor(GetTextureColor());
		}
		PlaceIcon(s, m_layers[i].icon, Fvector2().set(0.0f, 0.0f), b_rotate);
	}
}

// The layers below the weapon (below) or the others, in the order of inv_layers.
void CUIWeaponCellItem::DrawLayers(bool below)
{
	for (u32 i = 0; i < m_layers.size(); ++i)
		if (m_layer_icons[i] && m_layers[i].below == below)
			m_layer_icons[i]->Draw();
}
void CUIWeaponCellItem::RefreshOffset()
{
	if(object()->SilencerAttachable())
	{
		m_addon_offset[eSilencer].set(object()->GetSilencerX(), object()->GetSilencerY());
	}

	if(object()->ScopeAttachable())
	{
		m_addon_offset[eScope].set(object()->GetScopeX(), object()->GetScopeY());
		mScopeBack = object()->GetScopeBack();
	}

	if(object()->GrenadeLauncherAttachable())
	{
		m_addon_offset[eLauncher].set(object()->GetGrenadeLauncherX(), object()->GetGrenadeLauncherY());
	}
}

void CUIWeaponCellItem::Draw()
{
	DrawLayers(true);

	if (GetIcon(eScope) && mScopeBack)
	{
		GetIcon(eScope)->Draw();
		GetIcon(eScope)->SetAutoDelete(false);
		DetachChild(GetIcon(eScope));
	}

	inherited::Draw();

	if (GetIcon(eScope) && mScopeBack)
	{
		GetIcon(eScope)->SetAutoDelete(true);
		AttachChild(GetIcon(eScope));
	}


	if(m_upgrade && m_upgrade->IsShown())
		m_upgrade->Draw();
};

void CUIWeaponCellItem::Update()
{
	bool b						= Heading();
	inherited::Update			();
	
	bool bForceReInitAddons		= (b!=Heading());

	// The placed overlays carry the shift, so a new one places them all again.
	const u32 layers_on			= ActiveLayers();
	Fvector2 shift				= AttachedAddonsShift();
	shift.add					(LayersShift(layers_on));
	if (shift.x != m_shift.x || shift.y != m_shift.y)
	{
		m_shift					= shift;
		bForceReInitAddons		= true;
	}

	if (layers_on != m_layers_on || bForceReInitAddons)
	{
		m_layers_on				= layers_on;
		InitLayers				(Heading());
	}

	if (object()->SilencerAttachable())
	{
		if (object()->IsSilencerAttached())
		{
			if (!GetIcon(eSilencer) || bForceReInitAddons)
			{
				CreateIcon	(eSilencer);
				RefreshOffset();
				InitAddon	(GetIcon(eSilencer), eSilencer, Heading());
			}
		}
		else
		{
			if (m_addons[eSilencer])
				DestroyIcon(eSilencer);
		}
	}

	if (object()->ScopeAttachable())
	{
		if (m_addons[eScope])
		{
			DestroyIcon(eScope);
		}

		if (object()->IsScopeAttached())
		{
			if (!GetIcon(eScope) || bForceReInitAddons)
			{
				CreateIcon(eScope);
				RefreshOffset();
				InitAddon(GetIcon(eScope), eScope, Heading());
			}
		}
	}

	if (object()->GrenadeLauncherAttachable()){
		if (object()->IsGrenadeLauncherAttached())
		{
			if (!GetIcon(eLauncher) || bForceReInitAddons)
			{
				CreateIcon	(eLauncher);
				RefreshOffset();
				InitAddon	(GetIcon(eLauncher), eLauncher, Heading());
			}
		}
		else
		{
			if (m_addons[eLauncher])
				DestroyIcon(eLauncher);
		}
	}

	ApplyWeaponShift			();
}

void CUIWeaponCellItem::SetTextureColor( u32 color )
{
	inherited::SetTextureColor( color );
	if ( m_addons[eSilencer] )
	{
		m_addons[eSilencer]->SetTextureColor( color );
	}
	if ( m_addons[eScope] )
	{
		m_addons[eScope]->SetTextureColor( color );
	}
	if ( m_addons[eLauncher] )
	{
		m_addons[eLauncher]->SetTextureColor( color );
	}
	for (CUIStatic* s : m_layer_icons)
	{
		if (s)
			s->SetTextureColor(color);
	}
}

void CUIWeaponCellItem::OnAfterChild(CUIDragDropListEx* parent_list)
{
	if(is_silencer() && GetIcon(eSilencer))
		InitAddon	(GetIcon(eSilencer), eSilencer,	parent_list->GetVerticalPlacement());

	if(is_scope() && GetIcon(eScope))
		InitAddon	(GetIcon(eScope),	eScope,		parent_list->GetVerticalPlacement());

	if(is_launcher() && GetIcon(eLauncher))
		InitAddon	(GetIcon(eLauncher), eLauncher, parent_list->GetVerticalPlacement());

	InitLayers		(parent_list->GetVerticalPlacement());
}

void CUIWeaponCellItem::InitAddon(CUIStatic* s, eAddonType t, bool b_rotate)
{
	PlaceIcon(s, RefreshLayout(t), m_addon_offset[t], b_rotate);
}

// Places one overlay of the weapon's icon, an addon's or a layer's. legacy_offset (the
// addon's _x/_y) is used only while the layout is not custom.
void CUIWeaponCellItem::PlaceIcon(CUIStatic* s, const SAddonIconLayout& l, const Fvector2& legacy_offset, bool b_rotate)
{
	Frect tex_rect;
	Fvector2 base_scale;

	float scaleIcon = l.scale;
	if (Heading())
	{
		base_scale.x = GetHeight() / (INV_GRID_WIDTH(scaleIcon) * m_grid_size.x);
		base_scale.y = GetWidth() / (INV_GRID_HEIGHT(scaleIcon) * m_grid_size.y);
	}
	else
	{
		base_scale.x = GetWidth() / (INV_GRID_WIDTH(scaleIcon) * m_grid_size.x);
		base_scale.y = GetHeight() / (INV_GRID_HEIGHT(scaleIcon) * m_grid_size.y);
	}

	Fvector2 cell_size;
	cell_size.x = l.grid_size.x * INV_GRID_WIDTH(scaleIcon);
	cell_size.y = l.grid_size.y * INV_GRID_HEIGHT(scaleIcon);

	tex_rect.x1 = l.grid_pos.x * INV_GRID_WIDTH(scaleIcon);
	tex_rect.y1 = l.grid_pos.y * INV_GRID_HEIGHT(scaleIcon);

	tex_rect.rb.add(tex_rect.lt, cell_size);

	cell_size.mul(base_scale);

	// Legacy _x/_y come premultiplied by the weapon's inv_scale and base_scale divides by
	// the overlay's, which is right only while the two match. Custom ones are logical
	// units and get the overlay's own scale, which base_scale takes out again; so does
	// the shift of the whole icon.
	Fvector2 addon_offset = legacy_offset;
	if (l.custom)
		addon_offset.set(l.offset.x * scaleIcon, l.offset.y * scaleIcon);
	addon_offset.x += m_shift.x * scaleIcon;
	addon_offset.y += m_shift.y * scaleIcon;

	if (b_rotate)
	{
		s->SetWndSize(Fvector2().set(cell_size.y, cell_size.x));
		Fvector2 new_offset;
		new_offset.x = addon_offset.y * base_scale.y;
		new_offset.y = GetHeight() - addon_offset.x * base_scale.x - cell_size.x;
		addon_offset = new_offset;
		addon_offset.x *= UI().get_current_kx();
	} 
	else if (Heading() && !b_rotate)
	{
		addon_offset.mul(base_scale);
		addon_offset.x *= UI().get_current_kx();
		cell_size.x *= UI().get_current_kx();
		s->SetWndSize(cell_size);
	}
	else
	{
		s->SetWndSize(cell_size);
		addon_offset.mul(base_scale);
	}

	s->SetWndPos(addon_offset);
	s->SetTextureRect(tex_rect);
	s->SetStretchTexture(true);

	s->EnableHeading(b_rotate);

	if (b_rotate) {
		s->SetHeading(GetHeading());
		Fvector2 offs;
		offs.set(0.0f, s->GetWndSize().y);
		s->SetHeadingPivot(Fvector2().set(0.0f, 0.0f), /*Fvector2().set(0.0f,0.0f)*/offs, true);
	}
}

CUIDragItem* CUIWeaponCellItem::CreateDragItem()
{
	CUIDragItem* i		= inherited::CreateDragItem();
	CUIStatic* s		= nullptr;

	if (i)
	{
		const Fvector2 offset = weapon_shift_offset(m_shift, i->wnd()->GetWndSize(), m_grid_size, false, UI().get_current_kx());
		i->wnd()->SetTextureOffset(offset.x, offset.y);
		if (!Heading())
			i->wnd()->SetWndSize(weapon_sprite_size(i->wnd()->GetWndSize(), m_grid_size, m_base_grid));

		// The layers in the cell's order: the ones below the weapon go to the drag window
		// before its sprite, the others onto the sprite before the addon overlays.
		bool below		= false;
		for (u32 n = 0; n < m_layers.size(); ++n)
		{
			if (!(m_layers_on & (1u << n)))
				continue;
			s				= new CUIStatic(); s->SetAutoDelete(true);
			s->SetShader	(InventoryUtilities::GetEquipmentIconsShader(m_layers[n].icon.texture.c_str()));
			PlaceIcon		(s, m_layers[n].icon, Fvector2().set(0.0f, 0.0f), false);
			s->SetTextureColor(i->wnd()->GetTextureColor());
			if (m_layers[n].below)
			{
				i->AttachChild	(s);
				below		= true;
			}
			else
				i->wnd		()->AttachChild	(s);
		}
		if (below)
		{
			i->DetachChild	(i->wnd());
			i->AttachChild	(i->wnd());
		}
	}

	if(GetIcon(eSilencer))
	{
		s				= new CUIStatic(); s->SetAutoDelete(true);
		s->SetShader	(InventoryUtilities::GetEquipmentIconsShader(AddonIconsTexture(eSilencer)));
		InitAddon		(s, eSilencer, false);
		s->SetTextureColor(i->wnd()->GetTextureColor());
		i->wnd			()->AttachChild	(s);
	}
	
	if(GetIcon(eScope))
	{
		s				= new CUIStatic(); s->SetAutoDelete(true);
		s->SetShader	(InventoryUtilities::GetEquipmentIconsShader(AddonIconsTexture(eScope)));
		InitAddon		(s,	eScope, false);
		s->SetTextureColor(i->wnd()->GetTextureColor());
		i->wnd			()->AttachChild	(s);
	}

	if(GetIcon(eLauncher))
	{
		s				= new CUIStatic(); s->SetAutoDelete(true);
		s->SetShader	(InventoryUtilities::GetEquipmentIconsShader(AddonIconsTexture(eLauncher)));
		InitAddon		(s, eLauncher, false);
		s->SetTextureColor(i->wnd()->GetTextureColor());
		i->wnd			()->AttachChild	(s);
	}
	return				i;
}

bool CUIWeaponCellItem::EqualTo(CUICellItem* itm)
{
	if(!inherited::EqualTo(itm))	return false;

	CUIWeaponCellItem* ci			= smart_cast<CUIWeaponCellItem*>(itm);
	if(!ci)							return false;

//	bool b_addons					= ( (object()->GetAddonsState() == ci->object()->GetAddonsState()) );
	if ( object()->GetAddonsState() != ci->object()->GetAddonsState() )
	{
		return false;
	}
	if(this->is_scope() && ci->is_scope())
	{
		if ( object()->GetScopeName() != ci->object()->GetScopeName() )
		{
			return false;
		}
	}
//	bool b_place					= ( (object()->m_eItemCurrPlace == ci->object()->m_eItemCurrPlace) );

	return true;
}

CBuyItemCustomDrawCell::CBuyItemCustomDrawCell	(LPCSTR str, CGameFont* pFont)
{
	m_pFont		= pFont;
	VERIFY		(xr_strlen(str)<16);
	xr_strcpy		(m_string,str);
}

void CBuyItemCustomDrawCell::OnDraw(CUICellItem* cell)
{
	Fvector2							pos;
	cell->GetAbsolutePos				(pos);
	UI().ClientToScreenScaled			(pos, pos.x, pos.y);
	m_pFont->Out						(pos.x, pos.y, m_string);
	m_pFont->OnRender					();
}
