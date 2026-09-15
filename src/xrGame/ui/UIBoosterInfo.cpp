#include "stdafx.h"
#include "UIBoosterInfo.h"
#include "UIConditionFormat.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "object_broker.h"
#include "../EntityCondition.h"
#include "../Actor.h"
#include "../ActorCondition.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/UIHelper.h"
#include "../../xrEngine/string_table.h"

CUIBoosterInfo::CUIBoosterInfo()
{
	for(u32 i = 0; i < eBoostExplImmunity; ++i)
	{
		m_booster_items[i] = nullptr;
	}
	m_booster_satiety = nullptr;
	m_booster_thirst = nullptr;
	m_booster_sleepiness = nullptr;
	m_booster_anabiotic = nullptr;
	m_booster_time = nullptr;
}

CUIBoosterInfo::~CUIBoosterInfo()
{
	delete_data(m_booster_items);
	xr_delete(m_booster_satiety);
	xr_delete(m_booster_thirst);
	xr_delete(m_booster_sleepiness);
	xr_delete(m_booster_anabiotic);
	xr_delete(m_booster_time);
    xr_delete(m_satiety_note);
	if (m_Prop_line)
		xr_delete(m_Prop_line);
}

LPCSTR boost_influence_caption[] =
{
	"ui_inv_health",
	"ui_inv_power",
	"ui_inv_radiation",
	"ui_inv_bleeding",
	"ui_inv_outfit_additional_weight",
	"ui_inv_outfit_radiation_protection",
	"ui_inv_outfit_telepatic_protection",
	"ui_inv_outfit_chemical_burn_protection",
	"ui_inv_outfit_burn_immunity",
	"ui_inv_outfit_shock_immunity",
	"ui_inv_outfit_radiation_immunity",
	"ui_inv_outfit_telepatic_immunity",
	"ui_inv_outfit_chemical_burn_immunity"
};

void CUIBoosterInfo::InitFromXml(CUIXml& xml)
{
	LPCSTR base	= "booster_params";
	XML_NODE* stored_root = xml.GetLocalRoot();
	XML_NODE* base_node   = xml.NavigateToNode( base, 0 );
	if(!base_node)
		return;

	CUIXmlInit::InitWindow(xml, base, 0, this);
	xml.SetLocalRoot(base_node);
	
	if (xml.NavigateToNode("prop_line"))
	{
		m_Prop_line = UIHelper::CreateStatic(xml, "prop_line", this);
		m_Prop_line->SetAutoDelete(false);
	}

	for(u32 i = 0; i < eBoostExplImmunity; ++i)
	{
		if (xml.NavigateToNode(ef_boosters_section_names[i]))
		{
			m_booster_items[i] = new UIBoosterInfoItem();
			m_booster_items[i]->Init(xml, ef_boosters_section_names[i]);
			m_booster_items[i]->SetAutoDelete(false);

			LPCSTR name = g_pStringTable->translate(boost_influence_caption[i]).c_str();
			m_booster_items[i]->SetCaption(name);

			xml.SetLocalRoot(base_node);
		}
	}

	m_booster_satiety = new UIBoosterInfoItem();
	m_booster_satiety->Init(xml, "boost_satiety");
	m_booster_satiety->SetAutoDelete(false);
	LPCSTR name = g_pStringTable->translate("ui_inv_satiety").c_str();
	m_booster_satiety->SetCaption(name);
	xml.SetLocalRoot( base_node );

	if (xml.NavigateToNode("boost_thirst"))
	{
		m_booster_thirst = new UIBoosterInfoItem();
		m_booster_thirst->Init(xml, "boost_thirst");
		m_booster_thirst->SetAutoDelete(false);
		m_booster_thirst->SetCaption(g_pStringTable->translate("ui_inv_thirst").c_str());
		xml.SetLocalRoot(base_node);
	}

	if (xml.NavigateToNode("boost_sleepiness"))
	{
		m_booster_sleepiness = new UIBoosterInfoItem();
		m_booster_sleepiness->Init(xml, "boost_sleepiness");
		m_booster_sleepiness->SetAutoDelete(false);
		m_booster_sleepiness->SetCaption(g_pStringTable->translate("ui_inv_sleepiness").c_str());
		xml.SetLocalRoot(base_node);
	}

	if (xml.NavigateToNode("boost_anabiotic"))
	{
		m_booster_anabiotic = new UIBoosterInfoItem();
		m_booster_anabiotic->Init(xml, "boost_anabiotic");
		m_booster_anabiotic->SetAutoDelete(false);
		name = g_pStringTable->translate("ui_inv_survive_surge").c_str();
		m_booster_anabiotic->SetCaption(name);
		xml.SetLocalRoot(base_node);
	}

	if (xml.NavigateToNode("boost_time"))
	{
		m_booster_time = new UIBoosterInfoItem();
		m_booster_time->Init(xml, "boost_time");
		m_booster_time->SetAutoDelete(false);
		name = g_pStringTable->translate("ui_inv_effect_time").c_str();
		m_booster_time->SetCaption(name);
	}

    xml.SetLocalRoot(base_node);
    if (xml.NavigateToNode("regeneration_note"))
    {
        m_satiety_note = UIHelper::CreateTextWnd(xml, "regeneration_note", this);
        m_satiety_note->SetAutoDelete(false);
        m_satiety_note->SetTextST("ui_uip_item_reg_satiety");
        m_satiety_note->AdjustHeightToText();
    }
	xml.SetLocalRoot( stored_root );
}

void CUIBoosterInfo::SetInfo( shared_str const& section )
{
	DetachAll();
	if (m_Prop_line)
		AttachChild( m_Prop_line );

	CActor* actor = Level().CurrentViewEntity()->cast_actor();
	if (!actor)
	{
		return;
	}

	CEntityCondition::BOOSTER_MAP boosters = actor->conditions().GetCurBoosterInfluences();

	float val = 0.0f, max_val = 1.0f, h = 0.0f;
	Fvector2 pos;
	if (m_Prop_line)
		h = m_Prop_line->GetWndPos().y + m_Prop_line->GetWndSize().y;

	for (u32 i = 0; i < eBoostExplImmunity; ++i)
	{
		if(pSettings->line_exist(section.c_str(), ef_boosters_section_names[i]) && ef_boosters_section_names[i] && m_booster_items[i])
		{
			val	= pSettings->r_float(section, ef_boosters_section_names[i]);
			if ((i == eBoostHpRestore || i == eBoostPowerRestore) ? val == 0.0f : fis_zero(val))
				continue;

			EBoostParams type = (EBoostParams)i;
			switch(type)
			{
				case eBoostHpRestore: 
				case eBoostPowerRestore: 
				case eBoostBleedingRestore: 
				case eBoostMaxWeight: 
					max_val = 1.0f;
					break;
				case eBoostRadiationRestore: 
					max_val = -1.0f;
					break;
				case eBoostBurnImmunity: 
					max_val = actor->conditions().GetZoneMaxPower(ALife::infl_fire);
					break;
				case eBoostShockImmunity: 
					max_val = actor->conditions().GetZoneMaxPower(ALife::infl_electra);
					break;
				case eBoostRadiationImmunity: 
				case eBoostRadiationProtection: 
					max_val = actor->conditions().GetZoneMaxPower(ALife::infl_rad);
					break;
				case eBoostTelepaticImmunity: 
				case eBoostTelepaticProtection: 
					max_val = actor->conditions().GetZoneMaxPower(ALife::infl_psi);
					break;
				case eBoostChemicalBurnImmunity: 
				case eBoostChemicalBurnProtection: 
					max_val = actor->conditions().GetZoneMaxPower(ALife::infl_acid);
					break;
			}
			val /= max_val;
            if ((type == eBoostHpRestore || type == eBoostPowerRestore) && ConditionUi::RegenerationUnitsEnabled())
            {
                const bool power = type == eBoostPowerRestore;
                m_booster_items[i]->SetCaption(g_pStringTable->translate(power
                    ? "ui_uip_item_reg_power" : "ui_uip_item_reg_health").c_str());
                m_booster_items[i]->SetRegenerationRate(val, power);
            }
            else if (type == eBoostRadiationRestore && ConditionUi::RadiationUnitsEnabled())
			{
				LPCSTR key = val < 0.0f ? "ui_uip_item_rad_removal" : "ui_uip_item_rad_increase";
				m_booster_items[i]->SetCaption(g_pStringTable->translate(key).c_str());
				m_booster_items[i]->SetRadiationRate(val);
			}
			else
				m_booster_items[i]->SetValue(val);

			pos.set(m_booster_items[i]->GetWndPos());
			pos.y = h;
			m_booster_items[i]->SetWndPos(pos);

			h += m_booster_items[i]->GetWndSize().y;
			AttachChild(m_booster_items[i]);
		}
	}

	if(pSettings->line_exist(section.c_str(), "eat_satiety"))
	{
		val	= pSettings->r_float(section, "eat_satiety");
		if(!fis_zero(val))
		{
			m_booster_satiety->SetValue(val);
			pos.set(m_booster_satiety->GetWndPos());
			pos.y = h;
			m_booster_satiety->SetWndPos(pos);

			h += m_booster_satiety->GetWndSize().y;
			AttachChild(m_booster_satiety);
		}
	}
	if (pSettings->line_exist(section.c_str(), "eat_thirst") && m_booster_thirst)
	{
		val = pSettings->r_float(section, "eat_thirst");
		if (!fis_zero(val))
		{
			m_booster_thirst->SetValue(val);
			pos.set(m_booster_thirst->GetWndPos());
			pos.y = h;
			m_booster_thirst->SetWndPos(pos);

			h += m_booster_thirst->GetWndSize().y;
			AttachChild(m_booster_thirst);
		}
	}


	if(pSettings->line_exist(section.c_str(), "eat_sleepiness") && m_booster_sleepiness)
	{
		val	= pSettings->r_float(section, "eat_sleepiness");
		if(!fis_zero(val))
		{
			m_booster_sleepiness->SetValue(val);
			pos.set(m_booster_sleepiness->GetWndPos());
			pos.y = h;
			m_booster_sleepiness->SetWndPos(pos);

			h += m_booster_sleepiness->GetWndSize().y;
			AttachChild(m_booster_sleepiness);
		}
	}

	if(!xr_strcmp(section.c_str(), "drug_anabiotic") && m_booster_anabiotic)
	{
		pos.set(m_booster_anabiotic->GetWndPos());
		pos.y = h;
		m_booster_anabiotic->SetWndPos(pos);

		h += m_booster_anabiotic->GetWndSize().y;
		AttachChild(m_booster_anabiotic);
	}

	if(pSettings->line_exist(section.c_str(), "boost_time") && m_booster_time)
	{
		val	= pSettings->r_float(section, "boost_time");
		if(!fis_zero(val))
		{
            if (ConditionUi::RegenerationUnitsEnabled())
                {
                    m_booster_time->SetCaption(g_pStringTable->translate("ui_uip_item_reg_duration").c_str());
                    m_booster_time->SetDuration(val);
                }
            else
                m_booster_time->SetValue(val);
			pos.set(m_booster_time->GetWndPos());
			pos.y = h;
			m_booster_time->SetWndPos(pos);

			h += m_booster_time->GetWndSize().y;
			AttachChild(m_booster_time);
		}
	}
    if (m_satiety_note && pSettings->line_exist(section, "boost_power_restore") &&
        pSettings->r_float(section, "boost_power_restore") != 0.0f)
    {
        m_satiety_note->SetWndPos(Fvector2().set(0.0f, h));
        AttachChild(m_satiety_note);
        h += m_satiety_note->GetHeight();
    }
	SetHeight(h);
}

/// ----------------------------------------------------------------

UIBoosterInfoItem::UIBoosterInfoItem()
{
	m_caption				= nullptr;
	m_value					= nullptr;
	m_magnitude				= 1.0f;
	m_show_sign				= false;
	
	m_unit_str._set			("");
	m_texture_minus._set	("");
	m_texture_plus._set		("");
}

UIBoosterInfoItem::~UIBoosterInfoItem()
{
}

void UIBoosterInfoItem::Init(CUIXml& xml, LPCSTR section)
{
	CUIXmlInit::InitWindow(xml, section, 0, this);
	xml.SetLocalRoot(xml.NavigateToNode(section));

	m_caption   = UIHelper::CreateStatic(xml, "caption", this);
	m_value     = UIHelper::CreateTextWnd(xml, "value",   this);
	m_magnitude = xml.ReadAttribFlt("value", 0, "magnitude", 1.0f);
	m_sign_inverse = (xml.ReadAttribInt( "value", 0, "sign_inverse", 0 ) == 1);
	m_show_sign = (xml.ReadAttribInt("value", 0, "show_sign", 1) == 1);
	
	LPCSTR unit_str = xml.ReadAttrib("value", 0, "unit_str", "");
	m_unit_str._set(g_pStringTable->translate(unit_str));
	
	LPCSTR texture_minus = xml.Read("texture_minus", 0, "");
	if(texture_minus && xr_strlen(texture_minus))
	{
		m_texture_minus._set(texture_minus);
		
		LPCSTR texture_plus = xml.Read("caption:texture", 0, "");
		m_texture_plus._set(texture_plus);
		VERIFY(m_texture_plus.size());
	}
}

void UIBoosterInfoItem::SetCaption(LPCSTR name)
{
	m_caption->TextItemControl()->SetText(name);
}

void UIBoosterInfoItem::SetValue(float value)
{
	value *= m_magnitude;
	string32 buf;
	if(m_show_sign)
		xr_sprintf(buf, "%+.0f", value);
	else
		xr_sprintf(buf, "%.0f", value);
	
	string256 str;
	if(m_unit_str.size())
		xr_strconcat(str, buf, " ", m_unit_str.c_str());
	else
		xr_strconcat(str, buf);

	m_value->SetText(str);

	bool positive = (value >= 0.0f);
	positive = (m_sign_inverse) ? !positive : positive;
	m_value->SetTextColor(color_rgba(170, 170, 170, 255));

	if(m_texture_minus.size())
	{
		if(positive)
			m_caption->InitTexture(m_texture_plus.c_str());
		else
			m_caption->InitTexture(m_texture_minus.c_str());
	}
}

void UIBoosterInfoItem::SetRadiationRate(float value)
{
	string64 text;
	ConditionUi::FormatRadiationRate(text, value);
	m_value->SetText(text);
	m_value->SetTextColor(ConditionUi::RadiationColor(value));
	// Radiation emission is harmful; absorption (a negative rate) is beneficial.
	if (m_texture_minus.size())
		m_caption->InitTexture(value > 0.0f ? m_texture_minus.c_str() : m_texture_plus.c_str());
}

void UIBoosterInfoItem::SetRegenerationRate(float value, bool satietyDependent)
{
    m_regeneration_rate = value;
    m_has_regeneration_rate = true;
    m_satiety_dependent = satietyDependent;
    if (satietyDependent && Actor())
        value = Actor()->conditions().PowerRestoreEffect(value);
    string64 text;
    ConditionUi::FormatRegenerationRate(text, value);
    m_value->SetText(text);
    m_value->SetTextColor(value < 0.0f ? color_rgba(210,50,50,255) : color_rgba(170,170,170,255));
    if (m_texture_minus.size())
        m_caption->InitTexture(value < 0.0f ? m_texture_minus.c_str() : m_texture_plus.c_str());
}

void UIBoosterInfoItem::SetDuration(float seconds)
{
    m_duration = true;
    m_duration_seconds = seconds;
    const float factor = IsGameTypeSingle() ? Device.time_factor() : 1.0f;
    string32 number;
    ConditionUi::FormatNumber(number, factor > 0.0f ? seconds / factor : seconds,
        ConditionUi::DecimalSeparator(), false);
    string64 text;
    xr_strconcat(text, number, " ", g_pStringTable->translate("ui_inv_seconds_short").c_str());
    m_value->SetText(text);
}

void UIBoosterInfoItem::Update()
{
    if (m_has_regeneration_rate)
        SetRegenerationRate(m_regeneration_rate, m_satiety_dependent);
    if (m_duration)
        SetDuration(m_duration_seconds);
    CUIWindow::Update();
}
