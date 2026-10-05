#pragma once
#include "../../xrUI/Widgets/UIWindow.h"

class CUIXml;
class CUIStatic;
class CUITextWnd;
class CUIProgressBar;
class CInventoryItem;

// Ammo tooltip: the cartridge's own parameters from its section. Three bars
// with their numbers (precision from k_disp, hit power k_hit, penetration
// k_ap on the scale of the "Armour" bar), then the optional lines and the
// cartridge weight and box size.
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
	enum EBar
	{
		eBarPrecision,
		eBarHit,
		eBarAp,
		eBarCount
	};

	enum ERow
	{
		eRowPellets,
		eRowPierces,
		eRowImpair,
		eRowTracer,
		eRowMass,
		eRowBox,
		eRowCount
	};

	struct SBar
	{
		CUIWindow*		wnd				= nullptr;
		CUIProgressBar*	bar				= nullptr;
		CUIStatic*		overflow		= nullptr;
		CUIStatic*		overflow_fill	= nullptr;
		CUITextWnd*		value			= nullptr;
	};

	struct SRow
	{
		CUIWindow*	wnd		= nullptr;
		CUITextWnd*	caption	= nullptr;
		CUITextWnd*	value	= nullptr;
	};

			void	Place				(CUIWindow* wnd, float& h);
			void	SetBar				(EBar bar, float pos, LPCSTR text, float& h);

	CUIStatic*		m_Prop_line			= nullptr;
	SBar			m_bars[eBarCount];
	SRow			m_rows[eRowCount];
	float			m_gap				= 0.0f;
};
