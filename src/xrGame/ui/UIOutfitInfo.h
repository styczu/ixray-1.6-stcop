#pragma once

#include "../../xrUI/Widgets/UIWindow.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIDoubleProgressBar.h"
#include "../../xrEngine/AI/alife_space.h"

class CCustomOutfit;
class CHelmet;
class CArmorBase;
class CUIDoubleProgressBar;
class CUIXml;
class CUIScrollView;

class CUIOutfitImmunity final : public CUIStatic
{
public:
					CUIOutfitImmunity	();
	virtual			~CUIOutfitImmunity	();

	enum class InitResult
	{
		Failed,
		Normal,
		Plain
	};

		InitResult	Init				( CUIXml& xml_doc, const char* section );
			void	SetCaption			( const char* name );
			void	SetProgressValue	( float cur, float comp );
			void	SetDefaultValuesPlain(float magnitude, const shared_str& unit);
			bool	GetLegacyMode		() { return m_legacy_mode; }
			void	SetAfValue			( float val ) { m_af_value = val;}
			// Ochrona srodowiskowa w punktach wspolnej skali (ui-param-bars) zamiast procentu.
			void	SetZoneProtection	( bool zone ) { m_zone_protection = zone; }
			// Nadpisuje sam tekst wartosci (np. nazwa klasy pancerza zamiast liczby).
			void	SetValueText		( const char* text );
			// Wartosc w punktach panelu (x100, liczba calkowita) z paskiem pelnym
			// przy bar_max i trojkatem przepelnienia powyzej (wezly overflow).
			void	SetPointsValue		( float cur, float comp, float bar_max );
			// Roznica od porownywanego przedmiotu w kolumnie <delta>: "+4" / "-3",
			// 0 = pusto (ui-armor-tooltip-keys).
			void	SetDelta			( int steps );
			void	SetDeltaColors		( u32 better, u32 worse ) { m_delta_better = better; m_delta_worse = worse; }

	virtual CUIWindow* ui_cast_window() { return this; }

protected:
	bool					m_legacy_mode = false;
	CUIStatic*				m_name; // texture + name
	CUIDoubleProgressBar*	m_progress;
	CUIStatic*				m_value; // 100%
	CUIStatic*				m_text_legacy; // 100%
	CUIStatic*				m_overflow = nullptr;
	CUIStatic*				m_overflow_fill = nullptr;
	CUIStatic*				m_delta = nullptr;
	u32						m_delta_better = 0;
	u32						m_delta_worse = 0;
	float					m_magnitude;
	float					m_af_value = 0.0f;
	bool					m_zone_protection = false;
	shared_str				m_unit_str;

	InitResult	InitPlain(CUIXml& xml, const char* section);

}; // class CUIOutfitImmunity

// -------------------------------------------------------------------------------------

// Bloki szczegolow pod parametrami stroju (ui-armor-tooltip-keys r2): kreska,
// naglowki i wiersze "nazwa  wartosc  roznica" z szablonow XML, ukladane od nowa
// przy kazdej aktualizacji dymka.
class CUIOutfitDetails final : public CUIWindow
{
public:
	virtual			~CUIOutfitDetails	();
			void	InitFromXml			( CUIXml& xml, u32 better, u32 worse );
			void	Clear				();
			void	Line				();
			void	Header				( const char* key );
			void	Row					( const char* caption, const char* value, int delta );
			void	Finish				();

protected:
	struct SRow
	{
		CUIWindow*	wnd = nullptr;
		CUIStatic*	caption = nullptr;
		CUIStatic*	value = nullptr;
		CUIStatic*	delta = nullptr;
	};
	CUIStatic*			m_line = nullptr;
	CUIStatic*			m_headers[3] = {};
	SRow				m_rows[18];
	u32					m_used_headers = 0;
	u32					m_used_rows = 0;
	float				m_height = 0.0f;
	u32					m_better = 0;
	u32					m_worse = 0;

	void				Place				( CUIWindow* wnd );
};

class CUIOutfitInfo final : public CUIWindow
{
public:
					CUIOutfitInfo		();
	virtual			~CUIOutfitInfo		();

			void 	InitFromXml			( CUIXml& xml_doc );
			void 	UpdateInfo			( CCustomOutfit* cur_outfit, CCustomOutfit* slot_outfit = NULL );	
			void 	UpdateInfo			( CHelmet* cur_helmet, CHelmet* slot_helmet = NULL );
			// Klawisze dymka (ui-armor-tooltip-keys), przed UpdateInfo: porownanie
			// z przedmiotem w slocie tylko z lewym Alt, stan 100% z Ctrl.
			void	SetKeys				( bool compare, bool as_new ) { m_compare_key = compare; m_as_new = as_new; }
			// Linia "Porownanie: <name>" (wezel <compare>); nullptr bez wezla.
			CUIStatic* SetCompareLine	( const char* name );
			// Bloki szczegolow pod parametrami stroju (wezel <details>); nullptr bez wezla.
			CUIWindow* Details			() const;
//			void	SetItem				(CCustomOutfit* outfit, u32 hitType, bool force_add);

    CUIOutfitImmunity* CreateItem(CUIXml& uiXml, const char* section,
        float magnitude, const shared_str& unit,
        shared_str translationId);

	virtual CUIWindow* ui_cast_window() { return this; }

protected:
	enum				{ max_count = ALife::eHitTypeMax-2 };
	
	CUIStatic*			m_caption;
	CUIStatic*			m_Prop_line;
	CUIOutfitImmunity*	m_items[max_count];
	CUIScrollView*		m_listWnd;
	// Dodatkowe wiersze spoza tablicy hit-typow.
	CUIOutfitImmunity*	m_impact_absorption = nullptr; // (1 - hit_fraction_actor)
	CUIOutfitImmunity*	m_stamina_impact = nullptr;    // (1 - power_loss)
	CUIOutfitImmunity*	m_durability = nullptr;        // trwalosc w punktach (ArmorDurability.h)

	void				ShowInList			( CUIOutfitImmunity* item, bool visible );
	void				UpdateDurability	( CArmorBase* cur, CArmorBase* slot );

	CUIStatic*			m_compare = nullptr;
	class CUIOutfitDetails* m_details = nullptr;
	void				UpdateDetails		( CArmorBase& cur, CArmorBase* slot );
	bool				m_compare_key = false;
	bool				m_as_new = false;
	float				Condition			( CArmorBase& item ) const;
	float				ItemProtection		( CArmorBase& item, ALife::EHitType type ) const;
	float				Mechanical			( CArmorBase& item ) const;
	float				MaxBoneArmor		( CArmorBase& item ) const;

}; // class CUIOutfitInfo
