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
	Fvector2	shift{};		// logical units, moves the whole icon while the addon is on
	bool		custom = false;	// an icon key is set: offsets are taken in logical units
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
	bool mScopeBack = false;
	void						CreateIcon					(eAddonType);
	void						DestroyIcon					(eAddonType);
	void						RefreshOffset				();
	const SAddonIconLayout&		RefreshLayout				(eAddonType);
	LPCSTR						AddonIconsTexture			(eAddonType);
	Fvector2					AttachedAddonsShift			();
	void						ApplyWeaponShift			();
	CUIStatic*					GetIcon						(eAddonType);
	void						InitAddon					(CUIStatic* s, eAddonType t, bool use_heading);
	bool						is_scope					();
	bool						is_silencer					();
	bool						is_launcher					();
public:
								CUIWeaponCellItem			(CWeapon* itm);
				virtual			~CUIWeaponCellItem			();
	virtual		void			Update						();
	virtual		void			Draw						();
	virtual		void			SetTextureColor				(u32 color);

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
