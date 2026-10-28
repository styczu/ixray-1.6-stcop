#pragma once

#include "../../xrUI/Widgets/UIWindow.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIDoubleProgressBar.h"
#include "../../xrEngine/AI/alife_space.h"

class CCustomOutfit;
class CHelmet;
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

	virtual CUIWindow* ui_cast_window() { return this; }

protected:
	bool					m_legacy_mode = false;
	CUIStatic*				m_name; // texture + name
	CUIDoubleProgressBar*	m_progress;
	CUIStatic*				m_value; // 100%
	CUIStatic*				m_text_legacy; // 100%
	CUIStatic*				m_overflow = nullptr;
	CUIStatic*				m_overflow_fill = nullptr;
	float					m_magnitude;
	float					m_af_value = 0.0f;
	bool					m_zone_protection = false;
	shared_str				m_unit_str;

	InitResult	InitPlain(CUIXml& xml, const char* section);

}; // class CUIOutfitImmunity

// -------------------------------------------------------------------------------------

class CUIOutfitInfo final : public CUIWindow
{
public:
					CUIOutfitInfo		();
	virtual			~CUIOutfitInfo		();

			void 	InitFromXml			( CUIXml& xml_doc );
			void 	UpdateInfo			( CCustomOutfit* cur_outfit, CCustomOutfit* slot_outfit = NULL );	
			void 	UpdateInfo			( CHelmet* cur_helmet, CHelmet* slot_helmet = NULL );
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

	void				ShowInList			( CUIOutfitImmunity* item, bool visible );

}; // class CUIOutfitInfo
