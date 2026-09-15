////////////////////////////////////////////////////////////////////////////
//	Module 		: UIActorStateInfo.h
//	Created 	: 15.02.2008
//	Author		: Evgeniy Sokolov
//	Description : UI actor state window class
////////////////////////////////////////////////////////////////////////////

#ifndef	UI_ACTOR_STATE_INFO_H_INCLUDED
#define UI_ACTOR_STATE_INFO_H_INCLUDED

#include "alife_space.h"
#include "../../xrUI/Widgets/UIHint.h"
#include "../../xrCore/FormatParsers/XML/xrXMLParser.h"

class CUIProgressBar;
class CUIProgressShape;
class CUIStatic;
class CUIFrameWindow;
class CUIXml;
class CUIArrow;
class CUIStackPanel;
class CInventoryOwner;
class CActor;
class CCustomOutfit;
class CHelmet;

class ui_actor_state_item;
class ui_actor_state_row;

// Prog przebicia dla sekcji sylwetki: 0 = to, co kryje glowe (helm albo
// zintegrowany kaptur kombinezonu), 1 = reszta. -1 = nic nie kryje.
// Tabela kosci siedzi w UIActorStateInfo.cpp - delegaty wyrazen wolaja to
// stad, zeby jej nie dublowac.
namespace ActorArmor
{
	float SectionValue(CActor* actor, int section);
}

class ui_actor_state_wnd final : public CUIWindow
{
private:
	typedef CUIWindow		inherited;

public:
	enum EStateType
	{
		stt_stamina = 0,
		stt_health,
		stt_bleeding,
		stt_radiation,
		stt_armor,
		stt_main,
		stt_fire,
		stt_radia,
		stt_acid,
		stt_psi,
		stt_wound,
		stt_fire_wound,
		stt_shock,
		stt_power,
		stt_satiety,
		stt_thirst,
		stt_sleep,
		stt_intoxication,
		stt_count,
		stt_invalid = stt_count
	};

private:
	ui_actor_state_item*	m_state[stt_count]{};
	UIHint*					m_hint_wnd = nullptr;
	UIHint*					m_environment_hint_wnd = nullptr;
	bool					m_listMode = false;
	CUIStackPanel*			m_stateList = nullptr;
	xr_vector<ui_actor_state_row*> m_listRows;

public:
							ui_actor_state_wnd	() = default;
	virtual					~ui_actor_state_wnd	();
			void			init_from_xml			( CUIXml& xml, const char* path );
			void			UpdateActorInfo			( CInventoryOwner* owner );
			void			UpdateHitZone			();

	virtual void			Draw					();
	virtual void			Show					( bool status );

	virtual CUIWindow* ui_cast_window() { return this; }

private:
			void			init_legacy_from_xml	( CUIXml& xml );
			void			init_list_from_xml		( CUIXml& xml );
			void			UpdateActorInfoLegacy	( CActor* actor );
			void			UpdateActorInfoList		( CActor* actor );
			void			SetListValue			( EStateType type, float normalized );
			void			update_round_states		(EStateType stt_type, float initial, float max_power);

			// Wiersz klasy pancerza: liczba "glowa/korpus" oraz tooltip
			// z rozbiciem na grupy kosci.
			void			UpdateArmorInfo			(CActor* actor, CCustomOutfit* outfit, CHelmet* helmet);

			// Podpowiedzi, ktore musza pokazac wyliczona liczbe (skazenie, krwawienie).
			// System wyrazen nie sklada napisow, wiec tekst powstaje w C++.
            void            UpdateBleedingInfo(CActor* actor);
			void			UpdateRateHints			(CActor* actor);
            void UpdateProtectionHints(CActor* actor);
            // Ochrona bojowa: rozszarpanie/uderzenie/wybuch (prog absolutny) i
            // balistyka (absorpcja pancerza + mnoznik trudnosci). Liczba, pasek,
            // trojkat i podpowiedz - wszystko w C++, bez wyrazen XML.
            void UpdateCombatProtection(CActor* actor, CCustomOutfit* outfit, CHelmet* helmet);

public:
			static EStateType ParseStateType		( LPCSTR name );

private:
	friend class ui_actor_state_row;
};

class ui_actor_state_row final : public UIHintWindow
{
	typedef UIHintWindow inherited;

	ui_actor_state_wnd::EStateType m_type = ui_actor_state_wnd::stt_invalid;
	CUIStatic* m_icon = nullptr;
	CUIStatic* m_caption = nullptr;
	CUIStatic* m_value = nullptr;
	float m_magnitude = 100.f;
	shared_str m_format = "%d%%";

public:
	struct LayoutDefaults
	{
		float rowHeight = 20.f;
		Fvector2 iconSize;
		u32 iconColor = 0;
		u32 captionColor = 0;
		u32 valueColor = 0;
		float magnitude = 100.f;
		LPCSTR format = "%d%%";
		LPCSTR font = nullptr;
		LPCSTR layout = "icon,caption,value";
		LPCSTR columns = nullptr;
		float pad = 4.f;
		float valueWidth = 42.f;
		LPCSTR captionAlign = "left";
		LPCSTR valueAlign = "right";
		bool hasCaptionX = false;
		float captionX = 0.f;
		bool hasCaptionY = false;
		float captionY = 0.f;
		bool hasValueX = false;
		float valueX = 0.f;
		bool hasValueY = false;
		float valueY = 0.f;
		bool hasIconX = false;
		float iconX = 0.f;
		bool hasIconY = false;
		float iconY = 0.f;
	};

			void	init_from_xml(CUIXml& xml, XML_NODE* node, float rowWidth, const LayoutDefaults& defaults, UIHint* hintWnd);
			void	set_value(float normalized);
			ui_actor_state_wnd::EStateType type() const { return m_type; }

	virtual CUIWindow* ui_cast_window() { return this; }
};

class ui_actor_state_item final : public UIHintWindow
{
	typedef UIHintWindow	inherited;

protected:
	CUIStatic*				m_static;
	CUIStatic*				m_value = nullptr;
    CUIStatic* m_overflow = nullptr;
    CUIStatic* m_overflow_fill = nullptr;
    CUIProgressBar* m_environmental_exposure = nullptr;
	CUIStatic*				m_static2;
	CUIStatic*				m_static3;
	CUIProgressShape*		m_sensor;
	CUIArrow*				m_arrow;
	CUIArrow*				m_arrow_shadow;
	float					m_magnitude;

public:
	CUIProgressBar*			m_progress;
    bool m_regeneration = false;
    bool m_bleeding = false;
    void set_bleeding(float intensity);
    void set_protection_overflow(float ratio);
    void set_environmental_exposure(ALife::EHitType type, float sourceRatio, float protectionRatio, float opacity);
    void set_regeneration(float percentPerSecond, float maximum);
					ui_actor_state_item		();
	virtual			~ui_actor_state_item	();
			void	init_from_xml			( CUIXml& xml, const char* path );
	
			void	set_value_text			( LPCSTR text );
			bool	set_text				( float value ); // 0..1
			bool	set_text_str			( LPCSTR text ); // dowolny napis w tym samym statyku
			bool	set_progress			( float value ); // 0..1
			bool	set_progress_shape		( float value ); // 0..1
			int		set_arrow				( float value ); // 0..1
			bool	show_static				( bool status, u8 number=1 );

	virtual CUIWindow* ui_cast_window() { return this; }
};

#endif // UI_ACTOR_STATE_INFO_H_INCLUDED
