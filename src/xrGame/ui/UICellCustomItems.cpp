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
// them the overlay is the legacy one. _weapon_shift_x/_y move the whole icon.
IC void read_addon_icon_layout(LPCSTR icon_section, LPCSTR prefix, LPCSTR addon_section, SAddonIconLayout& l)
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

	l.offset.set(0.0f, 0.0f);
	if (l.custom)
	{
		xr_sprintf(key, "%s_x", prefix);
		l.offset.x = float(pSettings->r_s32(icon_section, key));
		xr_sprintf(key, "%s_y", prefix);
		l.offset.y = float(pSettings->r_s32(icon_section, key));
	}

	l.shift.x = own("weapon_shift_x") ? pSettings->r_float(icon_section, key) : 0.0f;
	l.shift.y = own("weapon_shift_y") ? pSettings->r_float(icon_section, key) : 0.0f;
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
		read_addon_icon_layout(*icon_section, prefix, *addon_section, l);
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

	// The placed overlays carry the shift, so a new one places them all again.
	const Fvector2 shift		= AttachedAddonsShift();
	if (shift.x != m_shift.x || shift.y != m_shift.y)
	{
		m_shift					= shift;
		bForceReInitAddons		= true;
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
}

void CUIWeaponCellItem::OnAfterChild(CUIDragDropListEx* parent_list)
{
	if(is_silencer() && GetIcon(eSilencer))
		InitAddon	(GetIcon(eSilencer), eSilencer,	parent_list->GetVerticalPlacement());

	if(is_scope() && GetIcon(eScope))
		InitAddon	(GetIcon(eScope),	eScope,		parent_list->GetVerticalPlacement());

	if(is_launcher() && GetIcon(eLauncher))
		InitAddon	(GetIcon(eLauncher), eLauncher, parent_list->GetVerticalPlacement());
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
