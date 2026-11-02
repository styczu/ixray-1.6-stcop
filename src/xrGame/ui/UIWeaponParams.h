#pragma once
#include "../../xrUI/Widgets/UIWindow.h"

class CUIXml;
class CUIStatic;
class CUIDoubleProgressBar;
class CUIProgressBar;
class CInventoryItem;
class CWeapon;

// Weapon tooltip (ui-weapon-params): the rows (ammo, capacity, rate of fire,
// bullet speed, dispersion, effective range), the combat bars (damage, accuracy,
// handling, recoil), the wear group (the condition bar coloured like the slot's,
// accuracy hold, jam resistance, durability) and the ammo icons with the one the
// numbers are for lit. Everything is the weapon as it fires now; the bars compare
// with the weapon in the slot like the stock tooltip.
class CUIWeaponParams : public CUIWindow
{
public:
	enum EBar
	{
		eBarDamage,
		eBarAccuracy,
		eBarHandling,
		eBarRecoil,
		eBarHold,
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
		eRowCondition,	// only without the <condition> bar node (an older layout)
		eRowCount
	};

	// The bar groups: combat, wear.
	static constexpr u32 eFirstWearBar = eBarHold;

	// One bar: its score (0..100 on the bar, damage x100 with a full bar at
	// 150) and whether it has no finite value ("max": no misfires, no wear).
	struct SScore
	{
		float	value	= 0.0f;
		bool	max		= false;
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

	// What the bars are computed from, read off the weapon and its cartridge
	// (Read), so the scores (Score) can be taken for changed parameters too.
	struct SParams
	{
		float	hit_power				= 0.0f;
		float	k_hit					= 1.0f;
		float	silencer_hit			= 1.0f;
		int		pellets					= 1;
		float	dispersion_deg			= 0.0f;	// fire_dispersion_base
		float	k_disp					= 1.0f;
		float	silencer_dispersion		= 1.0f;
		float	condition_factor		= 0.0f;	// fire_dispersion_condition_factor
		float	condition				= 1.0f;
		float	crosshair_inertion		= 0.0f;
		float	zoom_rotate_time		= 0.0f;
		float	control_inertion		= 1.0f;
		float	pdm_base				= 1.0f;	// PDM_disp_base
		float	pdm_velocity			= 1.0f;	// PDM_disp_vel_factor
		float	pattern_step			= 0.0f;	// mean recoil pattern step, 0: no pattern
		float	pattern_factor			= 0.0f;
		float	zoom_pattern_factor		= 0.0f;
		float	cam[4]					= {};	// hip, hip inc, zoom, zoom inc (radians, with the silencer)
		bool	legacy_misfire			= false;
		float	misfire_probability		= 0.0f;
		float	misfire_condition_k		= 0.0f;
		float	misfire_start_condition	= 0.0f;
		float	misfire_end_condition	= 0.0f;
		float	misfire_start_prob		= 0.0f;
		float	misfire_end_prob		= 0.0f;
		float	shot_dec				= 0.0f;	// condition_shot_dec
		float	impair					= 1.0f;
	};

	static	SAmmo	RifleAmmo			(CWeapon& weapon);
	static	void	Read				(CWeapon& weapon, const SAmmo& ammo, SParams& params);
	static	void	Score				(const SParams& params, SScore (&scores)[eBarCount], float& dispersion);

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
		CUIStatic*				value			= nullptr;
		float					full			= 100.0f;
	};

	struct SRow
	{
		CUIWindow*	wnd		= nullptr;
		CUIStatic*	caption	= nullptr;
		CUIStatic*	value	= nullptr;
	};

	struct SCondition
	{
		CUIWindow*		wnd		= nullptr;
		CUIProgressBar*	bar		= nullptr;
		CUIStatic*		value	= nullptr;
	};

	static	void	Measure				(CWeapon& weapon, const SAmmo& ammo, SScore (&scores)[eBarCount], float& dispersion);
	static	float	MeasureRange		(CWeapon& weapon, const SAmmo& ammo, float dispersion);
			void	Place				(CUIWindow* wnd, float& h);
			void	PlaceSeparator		(u32 index, float& h);
			void	SetRow				(ERow row, LPCSTR text, float& h);
			void	SetBar				(EBar bar, const SScore& cur, const SScore& slot, float& h);
			void	SetCondition		(float condition, float& h);
			void	SetAmmoIcons		(const SAmmo& ammo, float& h);

	CUIStatic*		m_Prop_line			= nullptr;
	CUIStatic*		m_separators[3]		= {};
	SCondition		m_condition;
	SBar			m_bars[eBarCount];
	SRow			m_rows[eRowCount];
	xr_vector<CUIStatic*> m_ammo_icons;
	float			m_gap				= 0.0f;
	float			m_icons_x			= 0.0f;
	float			m_icons_gap			= 0.0f;
	float			m_icons_cell		= 0.0f;
	u32				m_icon_dim_color	= 0;
};
