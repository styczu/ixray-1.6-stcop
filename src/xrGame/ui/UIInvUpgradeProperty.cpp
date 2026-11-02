////////////////////////////////////////////////////////////////////////////
//	Module 		: UIInvUpgradeProperty.cpp
//	Created 	: 22.11.2007
//  Modified 	: 13.03.2009
//	Author		: Evgeniy Sokolov, Prishchepa Sergey
//	Description : inventory upgrade property UIWindow class implementation
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "pch_script.h"
#include "UIInvUpgradeProperty.h"
#include "UIInvUpgradeInfo.h"
#include "UIConditionFormat.h"
#include "../ProtectionValues.h"
#include "../Actor.h"
#include "../ActorCondition.h"
#include "../CustomOutfit.h"
#include "../Weapon.h"
#include "../../xrEngine/xr_input.h"
#include "../../Include/xrRender/Kinematics.h"

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
	m_ui_text = new CUIStatic();
	AttachChild(m_ui_icon);
	AttachChild(m_ui_text);
	m_ui_icon->SetAutoDelete(true);
	m_ui_text->SetAutoDelete(true);

	CUIXmlInit::InitWindow(ui_xml, "properties", 0, this);
	SetWndPos(Fvector2().set(0, 0));
	CUIXmlInit::InitStatic(ui_xml, "properties:icon", 0, m_ui_icon);
	CUIXmlInit::InitStatic(ui_xml, "properties:text", 0, m_ui_text);
	m_text_color = m_ui_text->GetTextColor();
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

bool UIProperty::read_value_from_section(const char* section, const char* param, float& result)
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
	m_ui_text->SetTextColor(m_text_color);
	const int weapon = weapon_row();
	if (weapon >= 0)
	{
		return m_weapon_rows && show_weapon_row(m_weapon_rows[weapon]);
	}
	if (m_weapon_rows && is_weapon_label())
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
				const char* upgr_section = upgr->section();
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

// Tekst z addonu, bez tlumaczenia zastepstwo (jednostka jak w FormatProtectionPoints).
static shared_str TranslatedOr(LPCSTR id, LPCSTR fallback)
{
    const shared_str text = g_pStringTable->translate(id);
    return text.size() && xr_strcmp(text.c_str(), id) != 0 ? text : shared_str(fallback);
}

void UIProperty::set_armor_context(CInventoryItem* item, bool installed)
{
    m_armor_outfit = item ? item->cast_outfit() : nullptr;
    m_armor_installed = installed;
}

// prop_armor "Pancerz": skrypt pokazywal etykiete value ("Klasa pancerza
// +10%"), a ulepszenie podnosi progi przebicia kosci o pole armor sekcji
// bones_koeff_protection_add (SBoneProtections::add; Swit 0,41 -> 0,49).
// Wiersz to ten przyrost x100, skala liczby "Pancerz" panelu postaci, w
// strefach tooltipa klasy pancerza (Protection::ArmorGroups): najpierw
// sylwetka, potem glowa, w strefie najwiekszy przyrost jej kosci, strefy bez
// zmiany pominiete. fire_wound_protection sekcji nie wchodzi - strzal w aktora
// liczy tylko progi kosci.
// r8: z kombinezonem w kontekscie strefa to roznica liczb panelu (maksimum
// kosci z progiem >= 0, ArmorPoints) przed i po ulepszeniach, liczona na
// zywych progach przedmiotu: egzo 99 -> 99 na korpusie (obojczyki 0,99, a
// ulepszenie podnosi kregoslup 0,98 -> 0,99) nie trafia do wiersza. Bez stanu
// przedmiotu, jak inne wiersze okna. Bez kontekstu zostaje przyrost kosci.
bool UIProperty::show_armor(LPCSTR values)
{
    if (!get_property())
        return false;

    float sums[Protection::ArmorGroupCount][8] = {};
    bool found = false;
    string256 section;
    const int count = _GetItemCount(values);
    for (int i = 0; i < count; ++i)
    {
        _GetItem(values, i, section);
        if (!pSettings->section_exist(section) || !pSettings->line_exist(section, "bones_koeff_protection_add"))
            continue;
        LPCSTR bones = pSettings->r_string(section, "bones_koeff_protection_add");
        if (!bones || !*bones || !pSettings->section_exist(bones))
            continue;
        found = true;
        for (const auto& item : pSettings->r_section(bones).Data)
        {
            for (unsigned g = 0; g < Protection::ArmorGroupCount; ++g)
            {
                for (int b = 0; Protection::ArmorGroups[g].bones[b] != nullptr; ++b)
                {
                    if (xr_strcmp(item.first.c_str(), Protection::ArmorGroups[g].bones[b]) != 0)
                        continue;
                    string256 armor;
                    sums[g][b] += (float)atof(_GetItem(item.second.c_str(), 1, armor));
                }
            }
        }
    }
    if (!found)
        return false;

    IKinematics* kinematics = nullptr;
    if (m_armor_outfit && Level().CurrentViewEntity())
        kinematics = PKinematics(Level().CurrentViewEntity()->Visual());

    const char separator = ConditionUi::DecimalSeparator();
    string256 zones = "";
    for (int part = 1; part >= 0; --part)
    {
        for (unsigned g = 0; g < Protection::ArmorGroupCount; ++g)
        {
            const Protection::ArmorGroup& group = Protection::ArmorGroups[g];
            if (group.section != part)
                continue;
            float best = 0.0f;
            if (kinematics)
            {
                float before = -1.0f;
                float after = -1.0f;
                for (int b = 0; group.bones[b] != nullptr; ++b)
                {
                    const u16 bone_id = kinematics->LL_BoneID(group.bones[b]);
                    if (BI_NONE == bone_id)
                        continue;
                    const float live = m_armor_outfit->GetBoneArmor(bone_id);
                    const float base = m_armor_installed ? live - sums[g][b] : live;
                    before = std::max(before, base);
                    after = std::max(after, base + sums[g][b]);
                }
                // Strefa bez oslonietej kosci (-1, takze -1 + 0,11) liczy sie jako 0.
                best = 0.01f * (float)(Protection::ArmorPoints(std::max(after, 0.0f)) - Protection::ArmorPoints(std::max(before, 0.0f)));
            }
            else
            {
                for (int b = 0; group.bones[b] != nullptr; ++b)
                {
                    if (std::fabs(sums[g][b]) > std::fabs(best))
                        best = sums[g][b];
                }
            }
            // Zero po zaokragleniu do dwoch miejsc (FormatNumber) = strefa bez zmiany.
            if (std::fabs(best * 100.0f) < 0.005f)
                continue;
            string32 number;
            ConditionUi::FormatNumber(number, best * 100.0f, separator, true);
            const shared_str zone = TranslatedOr(group.zone_key, g_pStringTable->translate(group.label_key).c_str());
            if (zones[0])
                xr_strcat(zones, sizeof(zones), ", ");
            xr_strcat(zones, sizeof(zones), zone.c_str());
            xr_strcat(zones, sizeof(zones), " ");
            xr_strcat(zones, sizeof(zones), number);
        }
    }
    if (!zones[0])
        xr_strcpy(zones, sizeof(zones), "0");
    xr_strconcat(m_text, get_property()->name(), ": ", zones, " ", TranslatedOr("ui_uip_unit_protection", "pt").c_str());
    m_ui_text->SetText(m_text);
    return true;
}

// Wiersze fizyczne okna ulepszen liczone z kluczy sekcji efektow, jak ochrony
// srodowiskowe w compute_value (skrypt pokazuje jedna etykiete value na
// ulepszenie, wspolna dla wszystkich jego wlasciwosci).
// prop_damage: przyrost progow rozszarpania, uderzenia i wybuchu x100, skala
// panelu postaci; jedna liczba = przyrost sredniej trzech progow, jak
// "Ochrona mechaniczna" w dymku przedmiotu. prop_damage_absorption (nazwa
// sortuje sie zaraz po prop_damage): absorpcja kuli zatrzymanej przez
// pancerz w punktach procentowych, -100 x suma hit_fraction_actor, bez
// mnoznika trudnosci (jak dymek przedmiotu i panel "Balistyczna").
// prop_armor: patrz show_armor.
bool UIProperty::show_physical(LPCSTR values)
{
    if (xr_strcmp(m_property_id.c_str(), "prop_armor") == 0)
        return show_armor(values);

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

    const char separator = ConditionUi::DecimalSeparator();
    string64 points;
    string32 number;
    if (impact)
    {
        ConditionUi::FormatNumber(number, sums[0] * 100.0f, separator, true);
        xr_strconcat(points, number, " ", TranslatedOr("ui_uip_unit_pp", "pp").c_str());
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

// Wiersze broni (ui-upgrade-weapon-rows), w kolejnosci CUIWeaponParams::EUpgradeRow.
// Sekcje wlasciwosci z addonu (funktor bez tekstu), zadne ulepszenie ich nie
// deklaruje: wiersz liczy sie z faktycznej zmiany, nie z listy property.
static LPCSTR weapon_row_property[] =
{
	"prop_wpn_rpm", "prop_wpn_speed", "prop_wpn_accuracy", "prop_wpn_handling", "prop_wpn_recoil", "prop_wpn_hold",
	"prop_wpn_reliability", "prop_wpn_durability", "prop_wpn_weight", "prop_wpn_capacity", "prop_wpn_impulse"
};
static_assert(std::size(weapon_row_property) == CUIWeaponParams::eUpgradeRowCount);

// Etykiety skryptu (jedna wartosc value na wezel), ktore na broni zastepuja wiersze broni.
static LPCSTR weapon_label_property[] =
{
	"prop_rpm", "prop_bullet_speed", "prop_dispersion", "prop_inertion", "prop_recoil", "prop_reliability",
	"prop_weight", "prop_ammo_size"
};

int UIProperty::weapon_row() const
{
	for (int i = 0; i < CUIWeaponParams::eUpgradeRowCount; ++i)
	{
		if (xr_strcmp(m_property_id.c_str(), weapon_row_property[i]) == 0)
			return i;
	}
	return -1;
}

bool UIProperty::is_weapon_label() const
{
	for (LPCSTR id : weapon_label_property)
	{
		if (xr_strcmp(m_property_id.c_str(), id) == 0)
			return true;
	}
	return false;
}

// Zmiana miary dymka broni; bez wiersza, gdy po zaokragleniu jest zerem.
// Jednostki jak w dymku: paski w pkt, masa w kg z dwoma miejscami, impuls
// w % wartosci broni. Pasek bez skonczonej wartosci (bez zaciec, bez zuzycia)
// po ulepszeniu: "max". Pogorszenie (mniej, a przy masie wiecej) na czerwono,
// kolorem "Nie mozna zainstalowac" z UIInvUpgradeInfo.
bool UIProperty::show_weapon_row(const CUIWeaponParams::SUpgradeRow& row)
{
	const int index = weapon_row();
	shared_str unit;
	switch (index)
	{
	case CUIWeaponParams::eUpgradeRpm:		unit = g_pStringTable->translate("ui_uip_wpn_unit_rpm"); break;
	case CUIWeaponParams::eUpgradeSpeed:	unit = g_pStringTable->translate("ui_uip_wpn_unit_speed"); break;
	case CUIWeaponParams::eUpgradeWeight:	unit = g_pStringTable->translate("st_kg"); break;
	case CUIWeaponParams::eUpgradeCapacity:	unit = g_pStringTable->translate("ui_uip_wpn_unit_rounds"); break;
	case CUIWeaponParams::eUpgradeImpulse:	unit = "%"; break;
	default:								unit = TranslatedOr("ui_uip_unit_protection", "pt"); break;
	}

	const char separator = ConditionUi::DecimalSeparator();
	string32 number;
	if (row.before_max || row.after_max)
	{
		if (row.before_max == row.after_max)
			return false;
		if (row.after_max)
		{
			xr_strconcat(m_text, get_property()->name(), ": ", g_pStringTable->translate("ui_uip_wpn_max").c_str());
			m_ui_text->SetText(m_text);
			return true;
		}
		ConditionUi::FormatNumber(number, row.after, separator, false, 0);
		m_ui_text->SetTextColor(color_rgba(255, 90, 90, 255));
	}
	else
	{
		const int decimals = index == CUIWeaponParams::eUpgradeWeight ? 2 : 0;
		if (std::fabs(row.delta) < 0.5f * std::pow(10.0f, -float(decimals)))
			return false;
		ConditionUi::FormatNumber(number, row.delta, separator, true, decimals, decimals > 0);
		const float better = index == CUIWeaponParams::eUpgradeWeight ? -row.delta : row.delta;
		if (better < 0.0f)
			m_ui_text->SetTextColor(color_rgba(255, 90, 90, 255));
	}
	xr_strconcat(m_text, get_property()->name(), ": ", number, index == CUIWeaponParams::eUpgradeImpulse ? "" : " ", unit.c_str());
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

float UIProperty::GetRequiredWidth()
{
	m_ui_text->TextItemControl()->SetTextComplexMode(false);
	m_ui_text->AdjustWidthToText();
	return m_ui_text->GetWndPos().x + m_ui_text->GetWidth() + 1.0f;
}

void UIProperty::FitToWidth(float Width, bool WrapText)
{
	SetWidth(Width);
	m_ui_text->SetWidth(std::max(1.0f, Width - m_ui_text->GetWndPos().x - 1.0f));
	m_ui_text->TextItemControl()->SetTextComplexMode(WrapText);
	m_ui_text->SetHeight(m_ui_icon->GetHeight());
	if (WrapText)
	{
		m_ui_text->TextItemControl()->ParseText(true);
		m_ui_text->AdjustHeightToText();
		m_ui_text->SetHeight(std::max(m_ui_text->GetHeight(), m_ui_icon->GetHeight()));
	}
	SetHeight(std::max(m_ui_icon->GetWndPos().y + m_ui_icon->GetHeight(), m_ui_text->GetWndPos().y + m_ui_text->GetHeight()) + 1.0f);
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
	PendingRowHeight = std::max(PendingRowHeight, pWnd->GetHeight());
	counter += 1;

	// Устанавливаем вертикальное положение
	pos.y = h;

	// Если оба столбца заполнены (четное количество элементов), переходим на следующую строку
	if (counter % 2 == 0)
	{
		h += std::max(m_fnext_line_pos, PendingRowHeight);
		PendingRowHeight = 0.0f;
	}

	// Устанавливаем позицию элемента
	pWnd->SetWndPos(pos);
}

UIInvUpgPropertiesWnd::~UIInvUpgPropertiesWnd()
{
	delete_data(m_properties_ui);
}

void UIInvUpgPropertiesWnd::init_from_xml(const char* xml_name)
{
	CUIXml ui_xml;
	if (!ui_xml.Load( CONFIG_PATH, UI_PATH, xml_name ))
		return;
	
	XML_NODE* stored_root = ui_xml.GetLocalRoot();
	XML_NODE* node = ui_xml.NavigateToNode( "upgrade_info", 0 );
	ui_xml.SetLocalRoot( node );

	CUIXmlInit::InitWindow(ui_xml, "properties", 0, this);

	if (ui_xml.NavigateToNode("properties:upgr_line"))
	{
		m_Upgr_line = UIHelper::CreateStatic(ui_xml, "properties:upgr_line", this);
	}
	m_hint = UIHelper::CreateStatic(ui_xml, "properties:text", this);
	m_hint->Show(false);

	BaseWidth = GetWidth();
	AvailableWidth = BaseWidth;

	m_fsec_col_pos = ui_xml.ReadAttribFlt("properties", 0, "sec_col_pos", UI().is_widescreen() ? 105.f : 130.f);
	m_fnext_line_pos = ui_xml.ReadAttribFlt("properties", 0, "next_line_pos", 20.f);

	const char* properties_section = "upgrades_properties";

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
			m_has_weapon_rows |= ui_property->weapon_row() >= 0;
		} // for ib
	}
	ui_xml.SetLocalRoot(stored_root);
}

void UIInvUpgPropertiesWnd::SetAvailableWidth(float Width, bool AllowExpansion)
{
	AvailableWidth = std::max(1.0f, Width);
	CanExpand = AllowExpansion;
}

void UIInvUpgPropertiesWnd::set_info(ItemUpgrades_type const& item_upgrades, CInventoryItem* item, bool installed)
{
	// Bron: miary dymka przed i po tych ulepszeniach, raz dla wszystkich wierszy.
	CWeapon* Weapon = m_has_weapon_rows && item && ai().get_alife() ? smart_cast<CWeapon*>(item) : nullptr;
	if (Weapon)
	{
		xr_vector<LPCSTR> Sections;
		for (const shared_str& UpgradeId : item_upgrades)
			Sections.push_back(Level().m_upgrade_manager->get_upgrade(UpgradeId)->section());
		CUIWeaponParams::MeasureUpgrades(*Weapon, Sections, installed, m_weapon_values);
	}

	m_hint->Show(false);
	xr_vector<UIProperty*> VisibleProperties;
	float RequiredColumnWidth = 0.0f;
	for (auto* Property : m_properties_ui)
	{
		Property->Show(false);
		Property->set_armor_context(item, installed);
		Property->set_weapon_context(Weapon ? m_weapon_values : nullptr);
		if (Property->compute_value(item_upgrades))
		{
			VisibleProperties.push_back(Property);
			if (!Property->is_full_width_row())
				RequiredColumnWidth = std::max(RequiredColumnWidth, Property->GetRequiredWidth());
		}
	}
	// Wiersze broni na poczatku, w kolejnosci dymka broni; reszta jak w liscie.
	std::stable_sort(VisibleProperties.begin(), VisibleProperties.end(), [](UIProperty* Left, UIProperty* Right)
	{
		const int LeftRow = Left->weapon_row(), RightRow = Right->weapon_row();
		return LeftRow >= 0 && (RightRow < 0 || LeftRow < RightRow);
	});
	m_iNumUpgr = (int)VisibleProperties.size();
	PendingRowHeight = 0.0f;
	float Height = m_Upgr_line ? m_Upgr_line->GetWndSize().y + 1.0f : 0.0f;
	const float MinWidth = std::max(BaseWidth, AvailableWidth);
	const float ColumnWidth = CanExpand ? std::max(MinWidth * 0.5f, RequiredColumnWidth + 1.0f) : MinWidth * 0.5f;
	SetWidth(ColumnWidth * 2.0f);
	m_fsec_col_pos = ColumnWidth;

	for (u32 Index = 0; Index < VisibleProperties.size();)
	{
		UIProperty* Left = VisibleProperties[Index++];
		// Wiersz ochrony, regeneracji albo fizyczny idzie sam na cala szerokosc z zawijaniem;
		// zwykla wlasciwosc przed nim zostaje bez pary i tez bierze cala szerokosc.
		const bool FullRow = Left->is_full_width_row();
		UIProperty* Right = nullptr;
		if (!FullRow && Index < VisibleProperties.size() && !VisibleProperties[Index]->is_full_width_row())
			Right = VisibleProperties[Index++];

		Left->FitToWidth(Right ? ColumnWidth : GetWidth(), FullRow || !CanExpand);
		Left->SetWndPos(Fvector2().set(0.0f, Height));
		Left->Show(true);
		float RowHeight = Left->GetHeight();

		if (Right)
		{
			Right->FitToWidth(ColumnWidth, !CanExpand);
			Right->SetWndPos(Fvector2().set(ColumnWidth, Height));
			Right->Show(true);
			RowHeight = std::max(RowHeight, Right->GetHeight());
		}
		Height += std::max(m_fnext_line_pos, RowHeight);
	}
	SetHeight(Height);
}

void UIInvUpgPropertiesWnd::set_upgrade_info(Upgrade_type& upgrade, CInventoryItem* item)
{
	if (!upgrade.is_known())
	{
		SetWndSize(Fvector2().set(0, 0));
		return;
	}

	m_temp_upgrade_vector.resize(0);
	m_temp_upgrade_vector.push_back(upgrade.id());
	set_info(m_temp_upgrade_vector, item, item && item->has_upgrade(upgrade.id()));
}

void UIInvUpgPropertiesWnd::set_item_info(CInventoryItem& item)
{
	if (set_item_hint(item))
		return;
	set_info(item.upgardes(), &item, true);
}

// Dymek broni z wierszami broni: do 11 wierszy plus dodatki nie miesci sie na
// ekranie, wiec bez przytrzymanego Shift jest jedna linia "Ulepszenia: N
// (przytrzymaj Shift)". Dymek buduje sie co klatke (OnItemFocusedUpdate), wiec
// lista pojawia sie od razu po wcisnieciu. Bez tekstow addonu i przy padzie
// (bez Shift) zawsze pelna lista.
bool UIInvUpgPropertiesWnd::set_item_hint(CInventoryItem& item)
{
	if (!m_has_weapon_rows || !smart_cast<CWeapon*>(&item) || pInput->GetControllerMode() ||
		pInput->iGetAsyncKeyState(SDL_SCANCODE_LSHIFT) || pInput->iGetAsyncKeyState(SDL_SCANCODE_RSHIFT))
		return false;
	const shared_str label = g_pStringTable->translate("ui_uip_upgrades");
	const shared_str hold = g_pStringTable->translate("ui_uip_upgrades_shift");
	if (xr_strcmp(label.c_str(), "ui_uip_upgrades") == 0 || xr_strcmp(hold.c_str(), "ui_uip_upgrades_shift") == 0)
		return false;

	for (auto* Property : m_properties_ui)
		Property->Show(false);
	SetWidth(std::max(BaseWidth, AvailableWidth));
	string256 text;
	xr_sprintf(text, "%s: %u %s", label.c_str(), u32(item.upgardes().size()), hold.c_str());
	const float Top = m_Upgr_line ? m_Upgr_line->GetWndSize().y + 1.0f : 0.0f;
	m_hint->SetWndPos(Fvector2().set(m_hint->GetWndPos().x, Top));
	m_hint->SetWidth(GetWidth() - m_hint->GetWndPos().x);
	m_hint->SetText(text);
	m_hint->Show(true);
	m_iNumUpgr = 0;
	SetHeight(Top + std::max(m_fnext_line_pos, m_hint->GetHeight()));
	return true;
}

// Wiersze show_physical maja tekst jak ochrony srodowiskowe ("Nazwa: +N pkt"),
// wiec zajmuja cala szerokosc zamiast polowy kolumny.
bool UIProperty::is_physical() const
{
    return xr_strcmp(m_property_id.c_str(), "prop_damage") == 0 ||
        xr_strcmp(m_property_id.c_str(), "prop_damage_absorption") == 0 ||
        xr_strcmp(m_property_id.c_str(), "prop_armor") == 0;
}

bool UIProperty::is_regeneration() const
{
    return ConditionUi::RegenerationUnitsEnabled() &&
        (xr_strcmp(m_property_id.c_str(), "prop_restore_health") == 0 ||
         xr_strcmp(m_property_id.c_str(), "prop_power") == 0);
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
