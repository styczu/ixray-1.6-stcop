#pragma once

#include "../../xrUI/Widgets/UIWindow.h"
#include "../../xrUI/Widgets/UIDoubleProgressBar.h"
#include "../../xrEngine/AI/alife_space.h"

class CCustomOutfit;
class CHelmet;
class CUIStatic;
class CUIDoubleProgressBar;
class CUIXml;

class CUIOutfitImmunity : public CUIWindow
{
public:
					CUIOutfitImmunity	();
	virtual			~CUIOutfitImmunity	();

			bool	InitFromXml			( CUIXml& xml_doc, LPCSTR base_str, u32 hit_type );
			// Wiersz nie bedacy typem trafienia (np. absorpcja impaktu, wplyw
			// na zmeczenie): wlasna nazwa wezla XML i wlasny string etykiety,
			// prezentacja procentowa (jak wiersze non-zone).
			bool	InitFromNode		( CUIXml& xml_doc, LPCSTR base_str, LPCSTR node_name, LPCSTR st_name );
			void	SetProgressValue	( float cur, float comp );

	virtual CUIWindow* ui_cast_window() { return this; }

protected:
	// Wspolna sciezka inicjalizacji wiersza + uklad dwuliniowy (pasek pod
	// etykieta). node_name zapamietujemy, bo uklad odczytuje geometrie paska
	// z wlasnego wezla wiersza - dla nowych wierszy nie ma tablicy hit-typow.
			bool	InitRow				( CUIXml& xml_doc, LPCSTR base_str, LPCSTR node_name, LPCSTR st_name );
			void	ApplyTwoLineLayout	( CUIXml& xml_doc, LPCSTR base_str );

	CUIStatic				m_name; // texture + name
	CUIDoubleProgressBar	m_progress;
	CUITextWnd*				m_value; // 100%
	float					m_magnitude;
    bool m_zone_protection = false;
	shared_str				m_unit_str;
	string128				m_node_name;

}; // class CUIOutfitImmunity

// -------------------------------------------------------------------------------------

class CUIOutfitInfo : public CUIWindow
{
public:
					CUIOutfitInfo		();
	virtual			~CUIOutfitInfo		();

			void 	InitFromXml			( CUIXml& xml_doc );
			void 	UpdateInfo			( CCustomOutfit* cur_outfit, CCustomOutfit* slot_outfit = NULL );	
			void 	UpdateInfo			( CHelmet* cur_helmet, CHelmet* slot_helmet = NULL );

	virtual CUIWindow* ui_cast_window() { return this; }

protected:
	enum				{ max_count = ALife::eHitTypeMax-2 };
	
	CUIStatic*			m_caption;
	CUIStatic*			m_Prop_line;
	CUIOutfitImmunity*	m_items[max_count];
	// Dodatkowe wiersze spoza tablicy hit-typow.
	CUIOutfitImmunity*	m_impact_absorption = nullptr; // (1 - hit_fraction_actor)
	CUIOutfitImmunity*	m_stamina_impact = nullptr;    // (1 - power_loss)

}; // class CUIOutfitInfo
