#include "stdafx.h"
#include "UIAmmoParams.h"
#include "UIConditionFormat.h"
#include "../ProtectionValues.h"
#include "../inventory_item.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/UIHelper.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrEngine/string_table.h"

static LPCSTR ammo_bar_node[] =
{
	"precision",
	"hit",
	"ap",
};

static LPCSTR ammo_bar_caption[] =
{
	"ui_uip_ammo_precision",
	"ui_uip_ammo_hit",
	"ui_uip_ammo_ap",
};

static LPCSTR ammo_row_caption[] =
{
	"ui_uip_ammo_pellets",
	"ui_uip_ammo_pierces",
	"ui_uip_ammo_impair",
	"ui_uip_ammo_tracer",
	"ui_uip_ammo_mass",
	"ui_uip_ammo_box",
};

CUIAmmoParams::CUIAmmoParams()
{
}

CUIAmmoParams::~CUIAmmoParams()
{
	xr_delete(m_Prop_line);
	for (u32 i = 0; i < eBarCount; ++i)
		xr_delete(m_bars[i].wnd);
	for (u32 i = 0; i < eRowCount; ++i)
		xr_delete(m_rows[i].wnd);
}

// <ammo_params gap=""> holds one node per bar (<precision>, <hit>, <ap>: caption,
// progress with its own min/max, overflow, overflow_fill, value) and one <row>
// template (caption, value) for every other line.
void CUIAmmoParams::InitFromXml(CUIXml& xml)
{
	LPCSTR base = "ammo_params";
	XML_NODE* stored_root = xml.GetLocalRoot();
	XML_NODE* base_node = xml.NavigateToNode(base, 0);
	if (!base_node)
		return;

	CUIXmlInit::InitWindow(xml, base, 0, this);
	m_gap = xml.ReadAttribFlt(base, 0, "gap", 0.0f);
	xml.SetLocalRoot(base_node);

	if (xml.NavigateToNode("prop_line"))
	{
		m_Prop_line = UIHelper::CreateStatic(xml, "prop_line", nullptr);
		m_Prop_line->SetAutoDelete(false);
	}

	string256 buf;
	for (u32 i = 0; i < eBarCount; ++i)
	{
		SBar& bar = m_bars[i];
		LPCSTR node = ammo_bar_node[i];
		bar.wnd = new CUIWindow();
		bar.wnd->SetAutoDelete(false);
		CUIXmlInit::InitWindow(xml, node, 0, bar.wnd);
		xr_strconcat(buf, node, ":caption");
		UIHelper::CreateTextWnd(xml, buf, bar.wnd)->SetTextST(ammo_bar_caption[i]);
		bar.bar = new CUIProgressBar();
		bar.wnd->AttachChild(bar.bar);
		bar.bar->SetAutoDelete(true);
		xr_strconcat(buf, node, ":progress");
		CUIXmlInit::InitProgressBar(xml, buf, 0, bar.bar);
		xr_strconcat(buf, node, ":overflow");
		if (xml.NavigateToNode(buf))
			bar.overflow = UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":overflow_fill");
		if (xml.NavigateToNode(buf))
			bar.overflow_fill = UIHelper::CreateStatic(xml, buf, bar.wnd);
		xr_strconcat(buf, node, ":value");
		bar.value = UIHelper::CreateTextWnd(xml, buf, bar.wnd);
	}

	for (u32 i = 0; i < eRowCount; ++i)
	{
		SRow& row = m_rows[i];
		row.wnd = new CUIWindow();
		row.wnd->SetAutoDelete(false);
		CUIXmlInit::InitWindow(xml, "row", 0, row.wnd);
		row.caption = UIHelper::CreateTextWnd(xml, "row:caption", row.wnd);
		row.caption->SetTextST(ammo_row_caption[i]);
		row.value = UIHelper::CreateTextWnd(xml, "row:value", row.wnd);
	}

	xml.SetLocalRoot(stored_root);
}

bool CUIAmmoParams::Check(CInventoryItem& item)
{
	return m_bars[eBarAp].wnd && item.cast_weapon_ammo();
}

void CUIAmmoParams::Place(CUIWindow* wnd, float& h)
{
	Fvector2 pos = wnd->GetWndPos();
	pos.y = h;
	wnd->SetWndPos(pos);
	h += wnd->GetWndSize().y;
	AttachChild(wnd);
}

// Above the bar's maximum: full bar and the overflow triangle.
void CUIAmmoParams::SetBar(EBar index, float pos, LPCSTR text, float& h)
{
	SBar& bar = m_bars[index];
	bar.bar->SetProgressPosImmediate(pos);
	const bool overflow = pos > bar.bar->GetRange_max();
	if (bar.overflow)
		bar.overflow->Show(overflow);
	if (bar.overflow_fill)
		bar.overflow_fill->Show(overflow);
	bar.value->SetText(text);
	Place(bar.wnd, h);
}

void CUIAmmoParams::SetInfo(const shared_str& section)
{
	DetachAll();
	float h = 0.0f;
	if (m_Prop_line)
	{
		AttachChild(m_Prop_line);
		h = m_Prop_line->GetWndPos().y + m_Prop_line->GetWndSize().y;
	}
	LPCSTR sect = section.c_str();
	string32 number;
	string64 text;

	// Grenades and rockets (S_VOG25, S_M209, S_OG7B) are ammo too, but bullet
	// precision, hit power and penetration mean nothing for them.
	if (pSettings->r_clsid(sect, "class") == TEXT2CLSID("AMMO_S"))
	{
		const int pellets = pSettings->r_s32(sect, "buck_shot");

		// Precision = 1 + 99 * log10(5 / k_disp): k_disp 0.5 -> 100, 5.0 -> 1;
		// k_disp above 5 counts as 5, below 0.5 goes past 100.
		const float disp = _min(pSettings->r_float(sect, "k_disp"), 5.0f);
		if (disp > 0.0f)
		{
			const float precision = floorf(1.0f + 99.0f * log10f(5.0f / disp) + 0.5f);
			ConditionUi::FormatNumber(number, precision, ConditionUi::DecimalSeparator(), false, 0);
			SetBar(eBarPrecision, precision, number, h);
		}

		// Hit power: k_hit x 100; shot shows it per pellet.
		const float hit = pSettings->r_float(sect, "k_hit");
		ConditionUi::FormatNumber(number, hit * 100.0f, ConditionUi::DecimalSeparator(), false, 0);
		if (pellets > 1)
			xr_sprintf(text, "%s%s", number, g_pStringTable->translate("ui_uip_ammo_per_pellet").c_str());
		else
			xr_strcpy(text, number);
		SetBar(eBarHit, hit, text, h);

		// Same comparison as CCustomOutfit::HitThroughArmor: a bullet goes
		// through when k_ap is above the bone's armour, which the panel shows in
		// hundredths rounded half up (Protection::ArmorPoints, not required here).
		// Real cartridges stay at a few hundred; gauss's 999 only means "goes
		// through everything", so a four-digit number reads as "max".
		const float ap = pSettings->r_float(sect, "k_ap");
		if (ap >= 10.0f)
			xr_strcpy(text, g_pStringTable->translate("ui_uip_ammo_ap_max").c_str());
		else
			xr_sprintf(text, "%d", (int)floorf(ap * 100.0f + 0.5f));
		SetBar(eBarAp, ap, text, h);

		h += m_gap;

		if (pellets > 1)
		{
			xr_sprintf(text, "%d", pellets);
			m_rows[eRowPellets].value->SetText(text);
			Place(m_rows[eRowPellets].wnd, h);
		}

		// The best class it goes through. ArmorClassName counts the threshold
		// itself as reached, a bullet needs more: the thresholds have two
		// decimals, so half a hundredth below compares strictly. The first
		// class (threshold 0) is no armour at all, so it isn't worth a line.
		LPCSTR pierced = Protection::ArmorClassName(ap - 0.005f);
		LPCSTR none = Protection::ArmorClassName(0.0f);
		if (pierced && (!none || xr_strcmp(pierced, none) != 0))
		{
			xr_sprintf(text, "%s %s", g_pStringTable->translate("ui_uip_ammo_class_prefix").c_str(), pierced);
			m_rows[eRowPierces].value->SetText(text);
			Place(m_rows[eRowPierces].wnd, h);
		}

		const float impair = pSettings->r_float(sect, "impair");
		if (!fsimilar(impair, 1.0f))
		{
			const float percent = (impair - 1.0f) * 100.0f;
			ConditionUi::FormatNumber(number, percent, ConditionUi::DecimalSeparator(), true,
				_abs(percent) >= 0.5f ? 0 : 1);
			xr_sprintf(text, "%s%%", number);
			m_rows[eRowImpair].value->SetText(text);
			Place(m_rows[eRowImpair].wnd, h);
		}

		if (READ_IF_EXISTS(pSettings, r_bool, sect, "tracer", false))
		{
			m_rows[eRowTracer].value->SetTextST("ui_uip_ammo_yes");
			Place(m_rows[eRowTracer].wnd, h);
		}
	}

	// inv_weight is the full box (CWeaponAmmo::Weight scales it by the count).
	const int box = pSettings->r_s32(sect, "box_size");
	if (box > 0)
	{
		ConditionUi::FormatDetailNumber(number, pSettings->r_float(sect, "inv_weight") / box * 1000.0f,
			ConditionUi::DecimalSeparator(), false, 1);
		xr_sprintf(text, "%s %s", number, g_pStringTable->translate("ui_uip_ammo_unit_g").c_str());
		m_rows[eRowMass].value->SetText(text);
		Place(m_rows[eRowMass].wnd, h);

		xr_sprintf(text, "%d %s", box, g_pStringTable->translate("ui_uip_ammo_unit_pcs").c_str());
		m_rows[eRowBox].value->SetText(text);
		Place(m_rows[eRowBox].wnd, h);
	}

	SetHeight(h);
}
