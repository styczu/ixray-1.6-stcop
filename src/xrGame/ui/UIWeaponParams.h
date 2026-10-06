#pragma once
#include "../../xrUI/Widgets/UIWindow.h"

class CUIXml;
class CUIStatic;
class CUITextWnd;
class CUIDoubleProgressBar;
class CInventoryItem;
class CWeapon;

// Weapon tooltip (ui-weapon-params): the rows (ammo, capacity, rate of fire,
// bullet speed, dispersion, effective range, condition), six bars with their numbers and the
// ammo icons with the one the numbers are for lit. Everything is the weapon as it fires
// now; the bars compare with the weapon in the slot like the stock tooltip.
class CUIWeaponParams : public CUIWindow
{
public:
	enum EBar
	{
		eBarDamage,
		eBarAccuracy,
		eBarHandling,
		eBarRecoil,
		eBarReliability,
		eBarDurability,
		eBarCount
	};

	enum ERow
	{
		eRowAmmo,
		eRowCapacity,
		eRowRpm,
		eRowSpeed,
		eRowDispersion,
		eRowRange,
		eRowCondition,
		eRowCount
	};

	// One bar: its score (0..100 on the bar, damage x100 with a full bar at
	// 150) and whether it has no finite value ("max": no misfires, no wear).
	struct SScore
	{
		float	value	= 0.0f;
		bool	max		= false;
	};

					CUIWeaponParams		();
	virtual			~CUIWeaponParams	();
			void	InitFromXml			(CUIXml& xml);
			bool	Check				(CInventoryItem& item);
			void	SetInfo				(CInventoryItem* slot_item, CInventoryItem& item);
	virtual CUIWindow* ui_cast_window() { return this; }

private:
	struct SBar
	{
		CUIWindow*				wnd				= nullptr;
		CUIDoubleProgressBar*	bar				= nullptr;
		CUIStatic*				overflow		= nullptr;
		CUIStatic*				overflow_fill	= nullptr;
		CUITextWnd*				value			= nullptr;
		float					full			= 100.0f;
	};

	struct SRow
	{
		CUIWindow*	wnd		= nullptr;
		CUITextWnd*	caption	= nullptr;
		CUITextWnd*	value	= nullptr;
	};

	// The rifle side of the weapon: in grenade launcher mode the engine keeps
	// the rifle's ammo in the *2 members, the launcher is a weapon of its own.
	struct SAmmo
	{
		const xr_vector<shared_str>*	types		= nullptr;
		shared_str						section;		// the cartridge the numbers are for
		int								shown		= -1;	// its index in types: the lit icon
		int								capacity	= 0;
	};

	static	SAmmo	RifleAmmo			(CWeapon& weapon);
	static	void	Measure				(CWeapon& weapon, const SAmmo& ammo, SScore (&scores)[eBarCount], float& dispersion);
	static	float	MeasureRange		(CWeapon& weapon, const SAmmo& ammo, float dispersion);
			void	Place				(CUIWindow* wnd, float& h);
			void	PlaceSeparator		(u32 index, float& h);
			void	SetRow				(ERow row, LPCSTR text, float& h);
			void	SetBar				(EBar bar, const SScore& cur, const SScore& slot, float& h);
			void	SetAmmoIcons		(const SAmmo& ammo, float& h);

	CUIStatic*		m_Prop_line			= nullptr;
	CUIStatic*		m_separators[2]		= {};
	SBar			m_bars[eBarCount];
	SRow			m_rows[eRowCount];
	xr_vector<CUIStatic*> m_ammo_icons;
	float			m_gap				= 0.0f;
	float			m_icons_x			= 0.0f;
	float			m_icons_gap			= 0.0f;
	float			m_icons_cell		= 0.0f;
	u32				m_icon_dim_color	= 0;
};
