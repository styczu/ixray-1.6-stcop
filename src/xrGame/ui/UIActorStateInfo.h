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

class CUIProgressBar;
class CUIProgressShape;
class CUIStatic;
class CUIFrameWindow;
class CUIXml;
class CUIArrow;
class CInventoryOwner;
class CActor;
class CCustomOutfit;
class CHelmet;

class ui_actor_state_item;

// Prog przebicia dla sekcji sylwetki: 0 = to, co kryje glowe (helm albo
// zintegrowany kaptur kombinezonu), 1 = reszta. -1 = nic nie kryje.
// Tabela kosci siedzi w UIActorStateInfo.cpp - delegaty wyrazen wolaja to
// stad, zeby jej nie dublowac.
namespace ActorArmor
{
	float SectionValue(CActor* actor, int section);
}

class ui_actor_state_wnd : public CUIWindow
{
private:
	typedef CUIWindow		inherited;

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
		stt_count
	};
	ui_actor_state_item*	m_state[stt_count];
	UIHint*					m_hint_wnd;

public:
							ui_actor_state_wnd	() = default;
	virtual					~ui_actor_state_wnd	();
			void			init_from_xml			( CUIXml& xml, LPCSTR path );
			void			UpdateActorInfo			( CInventoryOwner* owner );
			void			UpdateHitZone			();

	virtual void			Draw					();
	virtual void			Show					( bool status );

	virtual CUIWindow* ui_cast_window() { return this; }

private:
			void			update_round_states		(EStateType stt_type, float initial, float max_power);

			// Wiersz klasy pancerza: liczba "glowa/korpus" oraz tooltip
			// z rozbiciem na grupy kosci.
			void			UpdateArmorInfo			(CActor* actor, CCustomOutfit* outfit, CHelmet* helmet);

			// Podpowiedzi, ktore musza pokazac wyliczona liczbe (skazenie, krwawienie).
			// System wyrazen nie sklada napisow, wiec tekst powstaje w C++.
			void			UpdateRateHints			(CActor* actor);

};

class ui_actor_state_item : public UIHintWindow
{
	typedef UIHintWindow	inherited;

protected:
	CUIStatic*				m_static;
	CUIStatic*				m_static2;
	CUIStatic*				m_static3;
	CUIProgressShape*		m_sensor;
	CUIArrow*				m_arrow;
	CUIArrow*				m_arrow_shadow;
	float					m_magnitude;

public:
	CUIProgressBar*			m_progress;
					ui_actor_state_item		();
	virtual			~ui_actor_state_item	();
			void	init_from_xml			( CUIXml& xml, LPCSTR path );
	
			bool	set_text				( float value ); // 0..1
			bool	set_text_str			( LPCSTR text ); // dowolny napis w tym samym statyku
			bool	set_progress			( float value ); // 0..1
			bool	set_progress_shape		( float value ); // 0..1
			int		set_arrow				( float value ); // 0..1
			bool	show_static				( bool status, u8 number=1 );

	virtual CUIWindow* ui_cast_window() { return this; }
}; // class ui_actor_state_item

#endif // UI_ACTOR_STATE_INFO_H_INCLUDED
