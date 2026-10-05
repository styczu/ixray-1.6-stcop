#pragma once
#include "../../xrUI/Widgets/UIWindow.h"

class CUIXml;
class CUIStatic;
class CUITextWnd;
class CUIProgressBar;
class CInventoryItem;

// Ammo tooltip: the cartridge's own parameters from its section. Penetration
// is a bar on the scale of the "Armour" bar (k_ap 1.0 = full); the rest are
// percentages of the weapon's own value, since the engine multiplies them in.
class CUIAmmoParams : public CUIWindow
{
public:
					CUIAmmoParams		();
	virtual			~CUIAmmoParams		();
			void	InitFromXml			(CUIXml& xml);
			bool	Check				(CInventoryItem& item);
			void	SetInfo				(const shared_str& section);
	virtual CUIWindow* ui_cast_window() { return this; }

private:
	enum ERow
	{
		eRowPellets,
		eRowTracer,
		eRowHit,
		eRowDist,
		eRowDisp,
		eRowImpair,
		eRowMass,
		eRowBox,
		eRowCount
	};

	struct SRow
	{
		CUIWindow*	wnd		= nullptr;
		CUITextWnd*	caption	= nullptr;
		CUITextWnd*	value	= nullptr;
	};

			void	Place				(CUIWindow* wnd, float& h);
			void	SetPercent			(ERow row, float percent);

	CUIStatic*		m_Prop_line			= nullptr;
	CUIWindow*		m_ap				= nullptr;
	CUIProgressBar*	m_ap_bar			= nullptr;
	CUIStatic*		m_ap_overflow		= nullptr;
	CUIStatic*		m_ap_overflow_fill	= nullptr;
	CUIWindow*		m_header			= nullptr;
	SRow			m_rows[eRowCount];
	float			m_gap				= 0.0f;
};
