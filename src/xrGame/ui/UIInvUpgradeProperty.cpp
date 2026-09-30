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
#include "../CustomOutfit.h"
#include "../ActorHelmet.h"

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
            string64 points;
            ConditionUi::FormatProtectionPoints(points, Protection::DisplayRatio(
                Protection::EquipmentContribution(protectionValue, protectionType),
                actor->conditions().GetZoneMaxPower(protectionType)));
            xr_strconcat(m_text, g_pStringTable->translate(Protection::Caption(protectionType)).c_str(), ": ", points);
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

void UIProperty::set_item_context(CInventoryItem* item, bool installed)
{
    m_has_item = false;
    m_upgrades_installed = installed;
    if (CCustomOutfit* outfit = item ? item->cast_outfit() : nullptr)
    {
        m_item_hit_fraction = outfit->GetHitFractionActor();
        m_has_item = true;
    }
    else if (CHelmet* helmet = item ? item->cast_helmet() : nullptr)
    {
        m_item_hit_fraction = helmet->GetHitFractionActor();
        m_has_item = true;
    }
}

// Liczba calkowita, ktora panel postaci pokazuje w "Balistyczna" dla danej
// ochrony (ta sama funkcja formatujaca), zeby roznica zgadzala sie z ekranem.
static int PanelPercent(float protection)
{
    string64 text;
    ConditionUi::FormatProtectionPointsPlain(text, protection);
    return atoi(text);
}

// Wiersze fizyczne okna ulepszen liczone z kluczy sekcji efektow, jak ochrony
// srodowiskowe w compute_value (skrypt pokazuje jedna etykiete value na
// ulepszenie, wspolna dla wszystkich jego wlasciwosci).
// prop_damage: przyrost progow rozszarpania, uderzenia i wybuchu x100, skala
// panelu postaci; jedna liczba = przyrost sredniej trzech progow, jak
// "Ochrona mechaniczna" w dymku przedmiotu. prop_damage_absorption (nazwa
// sortuje sie zaraz po prop_damage): absorpcja kuli zatrzymanej przez
// pancerz w punktach procentowych,
// -100 x suma hit_fraction_actor. Z kontekstem przedmiotu absorpcja
// uwzglednia mnoznik trudnosci (fire_wound_immunity) i jest roznica dwoch
// wartosci panelu "Balistyczna" (1 - hit_fraction x mnoznik) przed i po
// ulepszeniach, wiec zgadza sie z panelem takze w zaokragleniu.
bool UIProperty::show_physical(LPCSTR values)
{
    const bool mechanical = xr_strcmp(m_property_id.c_str(), "prop_damage") == 0;
    const bool impact = xr_strcmp(m_property_id.c_str(), "prop_damage_absorption") == 0;
    if ((!mechanical && !impact) || !get_property())
        return false;

    static const LPCSTR mechanical_keys[] = { "wound_protection", "strike_protection", "explosion_protection" };
    float sums[3] = {};
    bool found = false;
    string256 section;
    const int count = _GetItemCount(values);
    for (int i = 0; i < count; ++i)
    {
        _GetItem(values, i, section);
        float value;
        if (impact)
        {
            if (read_value_from_section(section, "hit_fraction_actor", value))
            {
                sums[0] -= value;
                found = true;
            }
            continue;
        }
        for (int k = 0; k < 3; ++k)
        {
            if (read_value_from_section(section, mechanical_keys[k], value))
            {
                sums[k] += value;
                found = true;
            }
        }
    }
    if (!found)
        return false;

    // Jednostka z tekstow addonu, bez tlumaczenia skrot jak w FormatProtectionPoints.
    auto unit = [](LPCSTR id, LPCSTR fallback)
    {
        const shared_str text = g_pStringTable->translate(id);
        return text.size() && xr_strcmp(text.c_str(), id) != 0 ? text : shared_str(fallback);
    };
    const char separator = ConditionUi::DecimalSeparator();
    string64 points;
    string32 number;
    if (impact)
    {
        float percent = sums[0] * 100.0f;
        CActor* actor = Level().CurrentViewEntity() ? Level().CurrentViewEntity()->cast_actor() : nullptr;
        if (m_has_item && actor)
        {
            const float mult = actor->conditions().GetHitImmunity(ALife::eHitTypeFireWound);
            const float change = -sums[0];  // suma hit_fraction_actor ulepszen
            const float after = m_upgrades_installed ? m_item_hit_fraction : m_item_hit_fraction + change;
            const float before = m_upgrades_installed ? m_item_hit_fraction - change : m_item_hit_fraction;
            percent = float(PanelPercent(1.0f - after * mult) - PanelPercent(1.0f - before * mult));
        }
        ConditionUi::FormatNumber(number, percent, separator, true);
        xr_strconcat(points, number, " ", unit("ui_uip_unit_pp", "pp").c_str());
    }
    else
    {
        // Przyrost sredniej trzech progow, jak "Ochrona mechaniczna" w dymku
        // przedmiotu (przy rownych przyrostach to po prostu ten przyrost).
        ConditionUi::FormatProtectionPoints(points, Protection::MechanicalMean(sums[0], sums[1], sums[2]));
    }
    xr_strconcat(m_text, get_property()->name(), ": ", points);
    m_ui_text->SetText(m_text);
    return true;
}

bool UIProperty::show_result(LPCSTR values)
{
	if (show_physical(values))
	{
		return true;
	}
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

void UIInvUpgPropertiesWnd::set_info(ItemUpgrades_type const& item_upgrades, bool supports_regeneration,
	CInventoryItem* item, bool installed)
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
        property->set_item_context(item, installed);
        if (!property->compute_value(item_upgrades))
            continue;
        if (property->is_regeneration() || property->is_protection() || property->is_physical())
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

void UIInvUpgPropertiesWnd::set_upgrade_info(Upgrade_type& upgrade, bool supports_regeneration, CInventoryItem* item)
{
	if (!upgrade.is_known())
	{
		SetWndSize(Fvector2().set(0, 0));
		return;
	}

	m_temp_upgrade_vector.resize(0);
	m_temp_upgrade_vector.push_back(upgrade.id());
	set_info(m_temp_upgrade_vector, supports_regeneration, item, item && item->has_upgrade(upgrade.id()));
}

void UIInvUpgPropertiesWnd::set_item_info(CInventoryItem& item)
{
	set_info(item.upgardes(), item.cast_helmet() == nullptr, &item, true);
}

// Wiersze show_physical maja tekst jak ochrony srodowiskowe ("Nazwa: +N pkt"),
// wiec zajmuja cala szerokosc zamiast polowy kolumny.
bool UIProperty::is_physical() const
{
    return xr_strcmp(m_property_id.c_str(), "prop_damage") == 0 ||
        xr_strcmp(m_property_id.c_str(), "prop_damage_absorption") == 0;
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
