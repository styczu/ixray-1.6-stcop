#pragma once
#include "UICellItem.h"
#include "../Weapon.h"


class CUIInventoryCellItem :public CUICellItem
{
	typedef  CUICellItem	inherited;
public:
								CUIInventoryCellItem		(CInventoryItem* itm);
	virtual		bool			EqualTo						(CUICellItem* itm);
	virtual		void			UpdateItemText				();
				CUIDragItem*	CreateDragItem				();
	virtual		bool			IsHelper					();
	virtual		void			SetIsHelper					(bool is_helper);
				bool			IsHelperOrHasHelperChild	();
				void			Update						();
				CInventoryItem* object						() {return (CInventoryItem*)m_pData;}

	virtual CUIWindow* ui_cast_window() { return this; }
	virtual CUIStatic* ui_cast_static() { return this; }
	virtual CUICellItem* ui_cast_cell_item() { return this; }
};

class CUIAmmoCellItem :public CUIInventoryCellItem
{
	typedef  CUIInventoryCellItem	inherited;
protected:
	virtual		void			 UpdateItemText				();
public:
								 CUIAmmoCellItem			(CWeaponAmmo* itm);

				u32				 CalculateAmmoCount			();
	virtual		bool			 EqualTo						(CUICellItem* itm);
	virtual		CUIDragItem*	 CreateDragItem				();
				CWeaponAmmo*	 object						() {return (CWeaponAmmo*)m_pData;}

	virtual CUIWindow* ui_cast_window() { return this; }
	virtual CUIStatic* ui_cast_static() { return this; }
	virtual CUICellItem* ui_cast_cell_item() { return this; }
};

// Inventory icon of one addon overlay, read from the weapon's (or scope's) section with
// the addon's own section as the fallback for every key.
struct SAddonIconLayout
{
	shared_str	icon_section;	// section the <prefix>_* keys were read from
	shared_str	addon_section;	// silencer_name, scope_name, grenade_launcher_name
	shared_str	texture;		// empty: the weapon's atlas
	Fvector2	grid_pos{};		// in atlas cells
	Fvector2	grid_size{};	// in atlas cells
	float		scale = 1.0f;	// atlas cell is 50 * scale px
	Fvector2	offset{};		// <prefix>_x/_y in logical units (50 per cell), used when custom
	Fvector2	shift{};		// <prefix>_weapon_offset_x/_y in logical units: moves the
								// whole icon while the addon is on
	Ivector2	extra{};		// <prefix>_weapon_extra_width/_height: cells the weapon's
								// cell grows by while the addon is on
	bool		custom = false;	// an icon key or addon_offsets_use_inv_scale is set: the
								// offset above is used instead of the legacy one
};

// One cosmetic layer of a weapon's inventory icon: a section listed in the weapon's
// inv_layers, drawn while all of its conditions hold.
struct SWeaponIconLayer
{
	shared_str				section;
	SAddonIconLayout		icon;			// always custom, texture always set
	bool					below = false;	// below_weapon
	xr_vector<shared_str>	upgrades;		// on only while one of them is installed
	xr_vector<shared_str>	no_upgrades;	// off while one of them is installed
	u8						no_addons = 0;	// off while one of these is on, a bit per
											// CUIWeaponCellItem::eAddonType
};

class CUIWeaponCellItem :public CUIInventoryCellItem
{
	typedef  CUIInventoryCellItem	inherited;
public:
	enum eAddonType{	eSilencer=0, eScope, eLauncher, eMaxAddon};
protected:
	CUIStatic*					m_addons					[eMaxAddon];
	Fvector2					m_addon_offset				[eMaxAddon];
	SAddonIconLayout			m_layout					[eMaxAddon];
	Fvector2					m_shift{};
	bool						m_scaled_offsets = false;	// addon_offsets_use_inv_scale
	Ivector2					m_base_grid;				// inv_grid_width/height
	xr_vector<SWeaponIconLayer>	m_layers;					// inv_layers, at most 32
	xr_vector<CUIStatic*>		m_layer_icons;				// one per layer, null while it is off
	u32							m_layers_on = 0;			// a bit per layer
	bool mScopeBack = false;
	void						CreateIcon					(eAddonType);
	void						DestroyIcon					(eAddonType);
	void						RefreshOffset				();
	const SAddonIconLayout&		RefreshLayout				(eAddonType);
	LPCSTR						AddonIconsTexture			(eAddonType);
	Fvector2					AttachedAddonsShift			();
	Ivector2					AttachedAddonsExtra			();
	void						ApplyWeaponShift			();
	u32							ActiveLayers				();
	Fvector2					LayersShift					(u32 layers_on);
	void						InitLayers					(bool b_rotate);
	void						DrawLayers					(bool below);
	CUIStatic*					GetIcon						(eAddonType);
	void						InitAddon					(CUIStatic* s, eAddonType t, bool use_heading);
	void						PlaceIcon					(CUIStatic* s, const SAddonIconLayout& l, const Fvector2& legacy_offset, bool b_rotate);
	bool						is_scope					();
	bool						is_silencer					();
	bool						is_launcher					();
public:
								CUIWeaponCellItem			(CWeapon* itm);
				virtual			~CUIWeaponCellItem			();
	virtual		void			Update						();
	virtual		void			Draw						();
	virtual		void			DrawTexture					();
	virtual		void			SetTextureColor				(u32 color);
	virtual		Ivector2		DesiredGridSize				(CUIDragDropListEx* list);

				CWeapon*		object						() {return (CWeapon*)m_pData;}
	virtual		void			OnAfterChild				(CUIDragDropListEx* parent_list);
	virtual		CUIDragItem*	CreateDragItem				();
	virtual		bool			EqualTo						(CUICellItem* itm);
	CUIStatic*					get_addon_static			(u32 idx)				{return m_addons[idx];}

	virtual CUIWindow* ui_cast_window() { return this; }
	virtual CUIStatic* ui_cast_static() { return this; }
	virtual CUICellItem* ui_cast_cell_item() { return this; }
};

class CBuyItemCustomDrawCell :public ICustomDrawCellItem
{
	CGameFont*			m_pFont;
	string16			m_string;
public:
						CBuyItemCustomDrawCell	(LPCSTR str, CGameFont* pFont);
	virtual void		OnDraw					(CUICellItem* cell);

};
