////////////////////////////////////////////////////////////////////////////
//	Module 		: UIInvUpgradeProperty.cpp
//	Created 	: 22.11.2007
//  Modified 	: 13.03.2009
//	Author		: Evgeniy Sokolov, Prishchepa Sergey
//	Description : inventory upgrade property UIWindow class implementation
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "pch_script.h"
#include "UIInvUpgradeProperty.h"
#include "UIInvUpgradeInfo.h"
#include "UIConditionFormat.h"
#include "../ProtectionValues.h"
#include "../Actor.h"
#include "../ActorCondition.h"

#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/xrUIXmlParser.h"
#include "../../xrUI/UIXmlInit.h"

#include "ai_space.h"
#include "alife_simulator.h"
#include "inventory_upgrade_manager.h"
#include "inventory_upgrade.h"
#include "inventory_upgrade_property.h"
#include "Level.h"
#include "../../xrUI/UIHelper.h"

void UIProperty::init_from_xml(CUIXml& ui_xml)
{
	m_ui_icon = new CUIStatic();
	m_ui_text = new CUITextWnd();
	AttachChild(m_ui_icon);
	AttachChild(m_ui_text);
	m_ui_icon->SetAutoDelete(true);
	m_ui_text->SetAutoDelete(true);

	CUIXmlInit::InitWindow(ui_xml, "properties", 0, this);
	SetWndPos(Fvector2().set(0, 0));
	CUIXmlInit::InitStatic(ui_xml, "properties:icon", 0, m_ui_icon);
	CUIXmlInit::InitTextWnd(ui_xml, "properties:text", 0, m_ui_text);
}

bool UIProperty::init_property(shared_str const& property_id)
{
	m_property_id = property_id;
	if (!get_property())
	{
		return false;
	}
	m_ui_icon->InitTexture(get_property()->icon_name());
	m_ui_icon->SetTextureColor(get_property()->icon_color());
	return true;
}

UIProperty::Property_type* UIProperty::get_property()
{
	if (!ai().get_alife())
	{
		return nullptr;
	}
	Property_type* proper = Level().m_upgrade_manager->get_property(m_property_id);
	VERIFY(proper);
	return proper;
}

bool UIProperty::read_value_from_section(LPCSTR section, LPCSTR param, float& result)
{
	result = 0.0f;
	if (!section || !pSettings->section_exist(section))
	{
		return false;
	}

	if (pSettings->line_exist(section, param) && *pSettings->r_string(section, param))
	{
		result = pSettings->r_float(section, param);
		return true;
	}
	return false;
}

bool UIProperty::compute_value(ItemUpgrades_type const& item_upgrades)
{
	if (!get_property())
	{
		return false;
	}

    const bool health = xr_strcmp(m_property_id.c_str(), "prop_restore_health") == 0;
    const bool regeneration = ConditionUi::RegenerationUnitsEnabled() &&
        (health || xr_strcmp(m_property_id.c_str(), "prop_power") == 0);
    float regenerationValue = 0.0f;
    const ALife::EHitType protectionType = protection_type();
    const bool protection = Protection::IsZoneType(protectionType);
    float protectionValue = 0.0f;
	int prop_count = 0;
	string2048 buf; buf[0] = 0;
	ItemUpgrades_type::const_iterator ib_upg = item_upgrades.begin();
	ItemUpgrades_type::const_iterator ie_upg = item_upgrades.end();
	for (; ib_upg != ie_upg; ++ib_upg)
	{
		Upgrade_type* upgr = Level().m_upgrade_manager->get_upgrade(*ib_upg);
		VERIFY(upgr);
		for (u8 i = 0; i < inventory::upgrade::max_properties_count; i++)
		{
			if (upgr->get_property_name(i)._get() == m_property_id._get())
			{
				LPCSTR upgr_section = upgr->section();
                if (protection)
                {
                    float value;
                    if (!read_value_from_section(upgr_section, Protection::ConfigKey(protectionType), value))
                        continue;
                    protectionValue += value;
                }
                if (regeneration)
                {
                    float value;
                    if (!read_value_from_section(upgr_section, health ? "health_restore_speed" : "power_restore_speed", value))
                        continue;
                    regenerationValue += value;
                }
				if (prop_count > 0)
				{
					xr_strcat(buf, sizeof(buf), ", ");
				}
				xr_strcat(buf, sizeof(buf), upgr_section);
				++prop_count;
			}
		}
	}
	if (prop_count > 0)
	{
        if (protection)
        {
            CActor* actor = Level().CurrentViewEntity() ? Level().CurrentViewEntity()->cast_actor() : nullptr;
            if (!actor)
                return false;
            string64 percent;
            ConditionUi::FormatProtectionPercent(percent, Protection::Ratio(
                Protection::EquipmentContribution(protectionValue, protectionType),
                actor->conditions().GetZoneMaxPower(protectionType)));
            xr_strconcat(m_text, g_pStringTable->translate(Protection::Caption(protectionType)).c_str(), ": ", percent);
            m_ui_text->SetText(m_text);
            return true;
        }
        if (regeneration)
        {
            string64 rate;
            ConditionUi::FormatRegenerationRate(rate, regenerationValue);
            xr_strconcat(m_text, g_pStringTable->translate(health ? "ui_uip_item_reg_health" : "ui_uip_item_reg_power").c_str(), ": ", rate);
            m_ui_text->SetText(m_text);
            return true;
        }
		return show_result(buf);
	}
	return false;
}

bool UIProperty::show_result(LPCSTR values)
{
	if (get_property() && get_property()->run_functor(values, m_text))
	{
		m_ui_text->SetText(m_text);
		return true;
	}
	else
	{
		m_ui_text->SetText("");
		return false;
	}
}

// =================== UIPropertiesWnd =====================================================

UIInvUpgPropertiesWnd::UIInvUpgPropertiesWnd()
{
	m_properties_ui.reserve(15);
	m_temp_upgrade_vector.reserve(1);
}

void UIInvUpgPropertiesWnd::UpdateStatsPos(float& h, Fvector2& pos, UIProperty* pWnd, int& counter) const
{
	// Если элемент четный, размещаем его в правом столбце
	if ((counter) % 2 == 0)
	{
		pos.x = 0.0f; // Левый столбец
	}
	else
	{
		pos.x = m_fsec_col_pos; // Правый столбец
	}
	// Увеличиваем счетчик
	counter += 1;

	// Устанавливаем вертикальное положение
	pos.y = h;

	// Если оба столбца заполнены (четное количество элементов), переходим на следующую строку
	if (counter % 2 == 0)
	{
		h += m_fnext_line_pos;
	}

	// Устанавливаем позицию элемента
	pWnd->SetWndPos(pos);
}

UIInvUpgPropertiesWnd::~UIInvUpgPropertiesWnd()
{
	delete_data(m_properties_ui);
}

void UIInvUpgPropertiesWnd::init_from_xml(LPCSTR xml_name)
{
	CUIXml ui_xml;
	ui_xml.Load(CONFIG_PATH, UI_PATH, xml_name);

	XML_NODE* stored_root = ui_xml.GetLocalRoot();
	XML_NODE* node = ui_xml.NavigateToNode("upgrade_info", 0);
	ui_xml.SetLocalRoot(node);

	CUIXmlInit::InitWindow(ui_xml, "properties", 0, this);

	if (ui_xml.NavigateToNode("properties:upgr_line"))
	{
		m_Upgr_line = UIHelper::CreateStatic(ui_xml, "properties:upgr_line", this);
	}

	m_fsec_col_pos = ui_xml.ReadAttribFlt("properties", 0, "sec_col_pos", UI().is_widescreen() ? 105.f : 130.f);
	m_fnext_line_pos = ui_xml.ReadAttribFlt("properties", 0, "next_line_pos", 20.f);

	LPCSTR properties_section = "upgrades_properties";

	shared_str property_id;

	if (pSettings->section_exist(properties_section))
	{
		CInifile::Sect& inv_section = pSettings->r_section(properties_section);

		for (const auto& section_data : inv_section.Data)
		{
			UIProperty* ui_property = new UIProperty(); // load one time !!
			ui_property->init_from_xml(ui_xml);

			property_id._set(section_data.first);
			if (!ui_property->init_property(property_id))
			{
				Msg("! Invalid property <%s> in inventory upgrade manager!", property_id.c_str());
				xr_delete(ui_property);
				continue;
			}

			m_properties_ui.push_back(ui_property);
			AttachChild(ui_property);
		} // for ib
	}
	ui_xml.SetLocalRoot(stored_root);
}

void UIInvUpgPropertiesWnd::set_info(ItemUpgrades_type const& item_upgrades, bool supports_regeneration)
{
    float h = m_Upgr_line ? m_Upgr_line->GetHeight() + 3.0f : 0.0f;
    float rowHeight = 0.0f;
    bool rightColumn = false;
    m_iNumUpgr = 0;
    for (auto& property : m_properties_ui)
    {
        property->Show(false);
        if (property->is_regeneration() && !supports_regeneration)
            continue;
        if (!property->compute_value(item_upgrades))
            continue;
        if (property->is_regeneration() || property->is_protection())
        {
            if (rightColumn)
            {
                h += rowHeight;
                rightColumn = false;
                rowHeight = 0.0f;
            }
            property->fit_full_width_row(GetWidth());
            property->SetWndPos(Fvector2().set(0.0f, h));
            h += _max(m_fnext_line_pos, property->GetHeight());
        }
        else
        {
            property->SetWndPos(Fvector2().set(rightColumn ? m_fsec_col_pos : 0.0f, h));
            rowHeight = _max(rowHeight, _max(m_fnext_line_pos, property->GetHeight()));
            if (rightColumn)
            {
                h += rowHeight;
                rowHeight = 0.0f;
            }
            rightColumn = !rightColumn;
        }
        property->Show(true);
        ++m_iNumUpgr;
    }
    SetHeight(h + (rightColumn ? rowHeight : 0.0f));
}

void UIInvUpgPropertiesWnd::set_upgrade_info(Upgrade_type& upgrade, bool supports_regeneration)
{
	if (!upgrade.is_known())
	{
		SetWndSize(Fvector2().set(0, 0));
		return;
	}

	m_temp_upgrade_vector.resize(0);
	m_temp_upgrade_vector.push_back(upgrade.id());
	set_info(m_temp_upgrade_vector, supports_regeneration);
}

void UIInvUpgPropertiesWnd::set_item_info(CInventoryItem& item)
{
	set_info(item.upgardes(), item.cast_helmet() == nullptr);
}

bool UIProperty::is_regeneration() const
{
    return ConditionUi::RegenerationUnitsEnabled() &&
        (xr_strcmp(m_property_id.c_str(), "prop_restore_health") == 0 ||
         xr_strcmp(m_property_id.c_str(), "prop_power") == 0);
}

void UIProperty::fit_full_width_row(float width)
{
    SetWidth(width);
    m_ui_text->SetWidth(_max(1.0f, width - m_ui_text->GetWndPos().x));
    m_ui_text->SetTextComplexMode(true);
    m_ui_text->AdjustHeightToText();
    SetHeight(_max(m_ui_icon->GetHeight(), m_ui_text->GetWndPos().y + m_ui_text->GetHeight()));
}

ALife::EHitType UIProperty::protection_type() const
{
    if (xr_strcmp(m_property_id.c_str(), "prop_thermo") == 0) return ALife::eHitTypeBurn;
    if (xr_strcmp(m_property_id.c_str(), "prop_electro") == 0) return ALife::eHitTypeShock;
    if (xr_strcmp(m_property_id.c_str(), "prop_chem") == 0) return ALife::eHitTypeChemicalBurn;
    if (xr_strcmp(m_property_id.c_str(), "prop_radio") == 0) return ALife::eHitTypeRadiation;
    if (xr_strcmp(m_property_id.c_str(), "prop_psy") == 0) return ALife::eHitTypeTelepatic;
    return ALife::eHitTypeMax;
}

bool UIProperty::is_protection() const
{
    return Protection::IsZoneType(protection_type());
}
