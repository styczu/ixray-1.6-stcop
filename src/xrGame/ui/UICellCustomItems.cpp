#include "StdAfx.h"
#include "UICellCustomItems.h"
#include "UIInventoryUtilities.h"
#include "../Weapon.h"
#include "UIDragDropListEx.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrUI/Widgets/UI3dStatic.h"
#include "../../xrUI/UIVectorBinding.h"

using namespace InventoryUtilities;

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
		// As GetInventoryIconParams: the addon's own icon cells may be fractional.
		return pSettings->r_float(addon_section, addon_key);
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
	return std::max(base, std::min(base + extra, capacity));
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


CUIInventoryCellItem::CUIInventoryCellItem(CInventoryItem* itm)
{
	m_pData = (void*)itm;

	const char* icons_texture = READ_IF_EXISTS(pSettings, r_string, itm->m_section_id, "icons_texture", nullptr);

	const bool isRaster = !(pSettings->line_exist(itm->m_section_id, kUIConfigField_InventoryVectorIcon));

	m_grid_size.set(itm->GetInvGridRect().rb);
	if (isRaster)
	{
		inherited::SetShader(GetEquipmentIconsShader(icons_texture));

		Frect rect;
		rect.lt.set(INV_GRID_WIDTH(itm->ScaleIcon) * itm->GetInvGridRect().x1,
			INV_GRID_HEIGHT(itm->ScaleIcon) * itm->GetInvGridRect().y1);

		rect.rb.set(rect.lt.x + INV_GRID_WIDTH(itm->ScaleIcon) * m_grid_size.x,
			rect.lt.y + INV_GRID_HEIGHT(itm->ScaleIcon) * m_grid_size.y);

		inherited::SetTextureRect(rect);
		inherited::SetStretchTexture(true);
	}
	else
	{
		Frect rect;
		rect.lt.set(INV_GRID_WIDTH(itm->ScaleIcon) * itm->GetInvGridRect().x1,
			INV_GRID_HEIGHT(itm->ScaleIcon) * itm->GetInvGridRect().y1);

		// todo: it is estimated but not real (true) values that render expects and when it applies to polygon so think about how to extract real values... (but probably in most cases should be fine and wouldn't be noticable, but if noticable so I refer to this todo so you need to use accurate and exact values for requested width and height)
		rect.rb.set(rect.lt.x + INV_GRID_WIDTH(itm->ScaleIcon) * m_grid_size.x,
			rect.lt.y + INV_GRID_HEIGHT(itm->ScaleIcon) * m_grid_size.y);
		const float fRequestedWidth = rect.width();
		const float fRequestedHeight = rect.height();

		Fvector2 vScaled;
		UI().ClientToScreenScaled(vScaled, fRequestedWidth, fRequestedHeight);

		vScaled.x = fRequestedWidth;
		vScaled.y = fRequestedHeight;

		xr_string_view iconSubpath{};
		if (pSettings->line_exist(itm->m_section_id, kUIConfigField_InventoryVectorIcon))
			iconSubpath = pSettings->r_string(itm->m_section_id, kUIConfigField_InventoryVectorIcon);

		CUIVectorBinding::ApplyVectorPathToStatic(*this, iconSubpath, vScaled.x, vScaled.y, {});
		inherited::SetStretchTexture(true);
	}

    // Для 3d иконок
	static const bool enable3DIcons = EngineExternal()[EEngineExternalGame::Enable3DIcons];
	if (enable3DIcons)
    {
		SetIconVisual(itm->m_3d_static_visual_name);
		SetBonesVisible(itm->object().Visual()->dcast_PKinematics());
    }
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
									+ (IsHelper() ? 1 : 0);

	const u32	count			=	ChildsCount() + 1 - helper_count;

    string32 tempStr;
    const char* finalText = nullptr;
    if (count > 1 || helper_count)
    {
        xr_sprintf(tempStr, "%s%d", EngineExternal().GetInventoryItemCountPrefix().c_str(), count);
        finalText = tempStr;
    }

    if (m_text)
	{
        m_text->Show(nullptr != finalText);
        m_text->SetText(finalText);
	}
	else
	{
		this->SetText(finalText);
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

// Full boxes leave the stack first, the partly filled one last: a drag, and a drop or a
// take of an amount, take the box of the top cell and the next top comes from the last
// child (PopChild), so the fullest box sits on top and the emptiest is the first child.
// Counts change in place (merging, unloading), so every frame. Only the boxes swap
// between the cells, as PopChild does; stacks with trade helpers keep their order.
void CUIAmmoCellItem::Update()
{
	ArrangeBoxes(true);
	inherited::Update();
}

// A move or a trade of an amount takes the children from the first and the top last:
// for it the fullest boxes go first among the children and the emptiest on top. The next
// Update puts the fullest back on top.
void CUIAmmoCellItem::ArrangeForChildMoves()
{
	ArrangeBoxes(false);
}

void CUIAmmoCellItem::ArrangeBoxes(bool fullest_on_top)
{
	bool helpers = IsHelper();
	for (CUICellItem* child : m_childs)
		helpers = helpers || child->IsHelper();
	if (m_childs.empty() || helpers)
		return;

	xr_vector<void*> boxes;
	boxes.push_back(m_pData);
	for (CUICellItem* child : m_childs)
		boxes.push_back(child->m_pData);
	// Fullest last for the top, else fullest first for the children.
	std::stable_sort(boxes.begin(), boxes.end(), [fullest_on_top](void* a, void* b)
	{
		const u16 ca = ((CWeaponAmmo*)a)->m_boxCurr, cb = ((CWeaponAmmo*)b)->m_boxCurr;
		return fullest_on_top ? ca < cb : ca > cb;
	});
	m_pData = boxes.back();
	for (u32 i = 0; i < m_childs.size(); ++i)
		m_childs[i]->m_pData = boxes[i];
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
    string32 tempStr;
    const char* finalText = nullptr;
	if ( !m_custom_draw )
	{
        xr_sprintf(tempStr, "%d", CalculateAmmoCount());
        finalText = tempStr;
    }
		
    if (m_text)
    {
        m_text->Show(nullptr != finalText);
        m_text->SetText(finalText);
    }
    else
    {
        this->SetText(finalText);
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
// keep the base size. 3D icons draw the model with its addons in the stock cell, which
// only the 3D icon editor resizes (SetGridSize), so they keep whatever size they have.
Ivector2 CUIWeaponCellItem::DesiredGridSize(CUIDragDropListEx* list)
{
	static const bool enable3DIcons = EngineExternal()[EEngineExternalGame::Enable3DIcons];
	if (enable3DIcons)
		return m_grid_size;

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
void CUIWeaponCellItem::DrawTexture()
{
	if (Heading() || (m_grid_size.x == m_base_grid.x && m_grid_size.y == m_base_grid.y))
	{
		inherited::DrawTexture();
		return;
	}

	const Fvector2 full = GetWndSize();
	SetWndSize(weapon_sprite_size(full, m_grid_size, m_base_grid));
	inherited::DrawTexture();
	SetWndSize(full);
}

// Moves only the drawn sprite: the cell, its hit area, count, condition bar and upgrade
// mark stay where the grid put them.
void CUIWeaponCellItem::ApplyWeaponShift()
{
	const Fvector2 offset = weapon_shift_offset(m_shift, GetWndSize(), m_grid_size, Heading(), UI().get_current_kx());
	SetTextureOffset(offset.x, offset.y);
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

	bool reinit_3d_icon = false;

	static const bool enable3DIcons = EngineExternal()[EEngineExternalGame::Enable3DIcons];

	// The placed overlays carry the shift, so a new one places them all again. 3D icons
	// draw the model with its addons and get no overlays, so they keep the stock sprite.
	Fvector2 shift;
	shift.set(0.0f, 0.0f);
	if (!enable3DIcons)
		shift					= AttachedAddonsShift();
	if (shift.x != m_shift.x || shift.y != m_shift.y)
	{
		m_shift					= shift;
		bForceReInitAddons		= true;
	}
	if (object()->SilencerAttachable())
	{
		if (object()->IsSilencerAttached() && !enable3DIcons)
		{
			if (!GetIcon(eSilencer) || bForceReInitAddons)
			{
				CreateIcon	(eSilencer);
				RefreshOffset();
				InitAddon	(GetIcon(eSilencer), eSilencer, Heading());

				reinit_3d_icon = true;
			}
        }
		else
		{
			if (m_addons[eSilencer])
			{
				DestroyIcon(eSilencer);
				reinit_3d_icon = true;
			}
		}
	}

	if (object()->ScopeAttachable())
	{
		if (m_addons[eScope])
		{
			DestroyIcon(eScope);
			reinit_3d_icon = true;
		}

		if (object()->IsScopeAttached() && !enable3DIcons)
		{
			if (!GetIcon(eScope) || bForceReInitAddons)
			{
				CreateIcon(eScope);
				RefreshOffset();
				InitAddon(GetIcon(eScope), eScope, Heading());
				reinit_3d_icon = true;
			}
		}
	}

	if (object()->GrenadeLauncherAttachable())
    {
		if (object()->IsGrenadeLauncherAttached() && !enable3DIcons)
		{
			if (!GetIcon(eLauncher) || bForceReInitAddons)
			{
				CreateIcon	(eLauncher);
				RefreshOffset();
				InitAddon	(GetIcon(eLauncher), eLauncher, Heading());
				reinit_3d_icon = true;
			}
		}
		else
		{
			if (m_addons[eLauncher])
			{
				DestroyIcon(eLauncher);
				reinit_3d_icon = true;
			}
		}
	}

  //	if (reinit_3d_icon) 
	{
		if (auto pItem = (CInventoryItem*)m_pData)
		{
			if (auto pWeapon = pItem->cast_weapon())
			{
				pWeapon->UpdateAddonsVisibility();
				pWeapon->ForceUpdateHUD();
				pWeapon->ProcessScope();
			}

			if (enable3DIcons)
			{
				SetIconVisual(pItem->m_3d_static_visual_name);
			}
			SetBonesVisible(pItem->object().Visual()->dcast_PKinematics());
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
}

void CUIWeaponCellItem::OnAfterChild(CUIDragDropListEx* parent_list)
{
	if(is_silencer() && GetIcon(eSilencer))
	{
		InitAddon(GetIcon(eSilencer), eSilencer, parent_list->GetVerticalPlacement());
	}

	if(is_scope() && GetIcon(eScope))
	{
		InitAddon(GetIcon(eScope), eScope, parent_list->GetVerticalPlacement());
	}

	if(is_launcher() && GetIcon(eLauncher))
	{
		InitAddon(GetIcon(eLauncher), eLauncher, parent_list->GetVerticalPlacement());
	}

	if (auto pItem = (CInventoryItem*)m_pData)
	{
		if (auto pWeapon = pItem->cast_weapon())
		{
			pWeapon->UpdateAddonsVisibility();
			pWeapon->ForceUpdateHUD();
			pWeapon->ProcessScope();
		}

		if (EngineExternal()[EEngineExternalGame::Enable3DIcons])
		{
			SetIconVisual(pItem->m_3d_static_visual_name);
		}
		SetBonesVisible(pItem->object().Visual()->dcast_PKinematics());
	}
}

void CUIWeaponCellItem::InitAddon(CUIStatic* s, eAddonType t, bool b_rotate)
{
	const SAddonIconLayout& l = RefreshLayout(t);
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
	Fvector2 addon_offset = m_addon_offset[t];
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

// Weapons never stack: each one keeps its own cell, so its loaded ammo can be seen
// (a stack showed the top weapon only, and nothing compared the ammo).
bool CUIWeaponCellItem::EqualTo(CUICellItem* itm)
{
	return false;
}

CBuyItemCustomDrawCell::CBuyItemCustomDrawCell	(const char* str, CGameFont* pFont)
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
