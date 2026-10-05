#include "stdafx.h"
#include "UIAmmoParams.h"
#include "UIConditionFormat.h"
#include "../inventory_item.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/UIHelper.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrEngine/string_table.h"

static LPCSTR ammo_row_caption[] =
{
	"ui_uip_ammo_pellets",
	"ui_uip_ammo_tracer",
	"ui_uip_ammo_hit",
	"ui_uip_ammo_dist",
	"ui_uip_ammo_disp",
	"ui_uip_ammo_impair",
	"ui_uip_ammo_mass",
	"ui_uip_ammo_box",
};

CUIAmmoParams::CUIAmmoParams()
{
}

CUIAmmoParams::~CUIAmmoParams()
{
	xr_delete(m_Prop_line);
	xr_delete(m_ap);
	xr_delete(m_header);
	for (u32 i = 0; i < eRowCount; ++i)
		xr_delete(m_rows[i].wnd);
}

// <ammo_params gap=""> holds the bar row <ap> (caption, progress, overflow,
// overflow_fill), the <header> of the weapon block and one <row> template
// (caption, value) for every other line.
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

	m_ap = new CUIWindow();
	m_ap->SetAutoDelete(false);
	CUIXmlInit::InitWindow(xml, "ap", 0, m_ap);
	UIHelper::CreateTextWnd(xml, "ap:caption", m_ap)->SetTextST("ui_uip_ammo_ap");
	m_ap_bar = new CUIProgressBar();
	m_ap->AttachChild(m_ap_bar);
	m_ap_bar->SetAutoDelete(true);
	CUIXmlInit::InitProgressBar(xml, "ap:progress", 0, m_ap_bar);
	if (xml.NavigateToNode("ap:overflow"))
		m_ap_overflow = UIHelper::CreateStatic(xml, "ap:overflow", m_ap);
	if (xml.NavigateToNode("ap:overflow_fill"))
		m_ap_overflow_fill = UIHelper::CreateStatic(xml, "ap:overflow_fill", m_ap);

	m_header = new CUIWindow();
	m_header->SetAutoDelete(false);
	CUIXmlInit::InitWindow(xml, "header", 0, m_header);
	UIHelper::CreateTextWnd(xml, "header:caption", m_header)->SetTextST("ui_uip_ammo_weapon");

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
	return m_ap && item.cast_weapon_ammo();
}

void CUIAmmoParams::Place(CUIWindow* wnd, float& h)
{
	Fvector2 pos = wnd->GetWndPos();
	pos.y = h;
	wnd->SetWndPos(pos);
	h += wnd->GetWndSize().y;
	AttachChild(wnd);
}

// Whole percent; below half a percent one or two decimals, so that a real
// difference does not round away to zero.
void CUIAmmoParams::SetPercent(ERow row, float percent)
{
	const float magnitude = _abs(percent);
	const int decimals = magnitude >= 0.5f ? 0 : (magnitude >= 0.05f ? 1 : 2);
	string32 number;
	ConditionUi::FormatNumber(number, percent, ConditionUi::DecimalSeparator(), true, decimals);
	string64 text;
	xr_sprintf(text, "%s%%", number);
	m_rows[row].value->SetText(text);
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

	// Grenades and rockets (S_VOG25, S_M209, S_OG7B) are ammo too, but bullet
	// penetration and weapon multipliers mean nothing for them.
	if (pSettings->r_clsid(sect, "class") == TEXT2CLSID("AMMO_S"))
	{
		// Same comparison as CCustomOutfit::HitThroughArmor: a bullet goes
		// through when k_ap is above the bone's armour, the number the panel
		// shows in hundredths. Above the bar's maximum: the overflow triangle.
		const float ap = pSettings->r_float(sect, "k_ap");
		m_ap_bar->SetProgressPosImmediate(ap);
		const bool overflow = ap > m_ap_bar->GetRange_max();
		if (m_ap_overflow)
			m_ap_overflow->Show(overflow);
		if (m_ap_overflow_fill)
			m_ap_overflow_fill->Show(overflow);
		Place(m_ap, h);

		const int pellets = pSettings->r_s32(sect, "buck_shot");
		if (pellets > 1)
		{
			string32 text;
			xr_sprintf(text, "%d", pellets);
			m_rows[eRowPellets].value->SetText(text);
			Place(m_rows[eRowPellets].wnd, h);
		}

		if (READ_IF_EXISTS(pSettings, r_bool, sect, "tracer", false))
		{
			m_rows[eRowTracer].value->SetTextST("ui_uip_ammo_yes");
			Place(m_rows[eRowTracer].wnd, h);
		}

		h += m_gap;
		Place(m_header, h);

		// Shot: k_hit is the weight of one pellet in the weapon's hit power.
		m_rows[eRowHit].caption->SetTextST(pellets > 1 ? "ui_uip_ammo_hit_pellet" : "ui_uip_ammo_hit");
		SetPercent(eRowHit, (pSettings->r_float(sect, "k_hit") - 1.0f) * 100.0f);
		Place(m_rows[eRowHit].wnd, h);

		SetPercent(eRowDist, (pSettings->r_float(sect, "k_dist") - 1.0f) * 100.0f);
		Place(m_rows[eRowDist].wnd, h);

		// k_disp scales the dispersion cone; grouping is its inverse.
		const float disp = pSettings->r_float(sect, "k_disp");
		if (disp > 0.0f)
		{
			SetPercent(eRowDisp, (1.0f / disp - 1.0f) * 100.0f);
			Place(m_rows[eRowDisp].wnd, h);
		}

		const float impair = pSettings->r_float(sect, "impair");
		if (!fsimilar(impair, 1.0f))
		{
			SetPercent(eRowImpair, (impair - 1.0f) * 100.0f);
			Place(m_rows[eRowImpair].wnd, h);
		}

		h += m_gap;
	}

	// inv_weight is the full box (CWeaponAmmo::Weight scales it by the count).
	const int box = pSettings->r_s32(sect, "box_size");
	if (box > 0)
	{
		string32 number;
		ConditionUi::FormatDetailNumber(number, pSettings->r_float(sect, "inv_weight") / box * 1000.0f,
			ConditionUi::DecimalSeparator(), false, 1);
		string64 text;
		xr_sprintf(text, "%s %s", number, g_pStringTable->translate("ui_uip_ammo_unit_g").c_str());
		m_rows[eRowMass].value->SetText(text);
		Place(m_rows[eRowMass].wnd, h);

		xr_sprintf(text, "%d %s", box, g_pStringTable->translate("ui_uip_ammo_unit_pcs").c_str());
		m_rows[eRowBox].value->SetText(text);
		Place(m_rows[eRowBox].wnd, h);
	}

	SetHeight(h);
}
