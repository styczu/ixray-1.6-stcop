#pragma once

#include "../xrEngine/AI/alife_space.h"
#include <cmath>

namespace Protection
{
    inline bool IsZoneType(ALife::EHitType type)
    {
        switch (type)
        {
        case ALife::eHitTypeBurn:
        case ALife::eHitTypeLightBurn: // The burn immunity also covers ambient heat.
        case ALife::eHitTypeShock:
        case ALife::eHitTypeChemicalBurn:
        case ALife::eHitTypeRadiation:
        case ALife::eHitTypeTelepatic:
            return true;
        default:
            return false;
        }
    }

    // GetDefHitTypeProtection already includes the item's condition.
    inline float EquipmentContribution(float conditionedProtection, ALife::EHitType type)
    {
        return conditionedProtection * (IsZoneType(type) ? 0.1f : 1.0f);
    }

    struct ThresholdReading
    {
        float power = 0.0f;
        bool attainable = false;
    };

    // UI only: invert the sequential clamps in outfit -> helmet -> flat booster.
    // For nonnegative layers this is (outfit + helmet + booster) / multiplier.
    // A negative later layer can make zero unattainable even at zero input.
    // Actor immunity deliberately does not enter the pre-immunity threshold.
    inline ThresholdReading EffectiveThreshold(float outfit, float helmet, float booster, float multiplier)
    {
        if (!(multiplier > 0.0f) || !std::isfinite(multiplier) ||
            !std::isfinite(outfit) || !std::isfinite(helmet) || !std::isfinite(booster))
            return {};
        float remaining = booster;
        if (remaining < 0.0f)
            return {};
        remaining += helmet;
        if (remaining < 0.0f)
            return {};
        remaining += outfit;
        if (remaining < 0.0f)
            return {};
        const float power = remaining / multiplier;
        return std::isfinite(power) ? ThresholdReading{power, true} : ThresholdReading{};
    }

    // UI only: how many times the belt raises the threshold, 1 / HitArtefactsOnBelt(1).
    // For nonnegative layers EffectiveThreshold = base threshold x influence.
    inline float ArtefactInfluence(float hitMultiplier)
    {
        return hitMultiplier > 0.0f && std::isfinite(hitMultiplier) ? 1.0f / hitMultiplier : 1.0f;
    }

    // UI only: colour direction of the influence as shown with two decimals,
    // so a value printed as x1 is never coloured. 1 = gain, -1 = loss, 0 = none.
    inline int InfluenceDirection(float influence)
    {
        const float shown = std::round(influence * 100.0f);
        return shown > 100.0f ? 1 : (shown < 100.0f ? -1 : 0);
    }

    // Presentation scale, not a percentage reduction of an arbitrary hit.
    // Keep values above 1 and below 0; only the graphical fill is clamped.
    inline float Ratio(float protection, float zoneMaxPower)
    {
        return zoneMaxPower > 0.0f ? protection / zoneMaxPower : 0.0f;
    }

    // UI only: the item tooltip's single "mechanical protection" value, the
    // mean of the three thresholds mechanical upgrades raise equally (x100 =
    // character panel points). Its bar is full at MechanicalBarMax (50 points);
    // stronger armour overflows with the panel's triangle.
    inline float MechanicalMean(float wound, float strike, float explosion)
    {
        return (wound + strike + explosion) / 3.0f;
    }
    constexpr float MechanicalBarMax = 0.5f;

    // UI only: 100 points = one tenth of the configured zone reference.
    // A linear score preserves addition and differences between items.
    // It is not a percentage of damage absorbed or an immunity threshold.
    inline float DisplayRatio(float protection, float zoneMaxPower)
    {
        return 10.0f * Ratio(protection, zoneMaxPower);
    }

    inline const char* ConfigKey(ALife::EHitType type)
    {
        switch (type)
        {
        case ALife::eHitTypeBurn:
        case ALife::eHitTypeLightBurn: return "burn_protection";
        case ALife::eHitTypeShock: return "shock_protection";
        case ALife::eHitTypeChemicalBurn: return "chemical_burn_protection";
        case ALife::eHitTypeRadiation: return "radiation_protection";
        case ALife::eHitTypeTelepatic: return "telepatic_protection";
        default: return nullptr;
        }
    }

    inline const char* Caption(ALife::EHitType type)
    {
        switch (type)
        {
        case ALife::eHitTypeBurn:
        case ALife::eHitTypeLightBurn: return "ui_inv_outfit_burn_protection";
        case ALife::eHitTypeShock: return "ui_inv_outfit_shock_protection";
        case ALife::eHitTypeChemicalBurn: return "ui_inv_outfit_chemical_burn_protection";
        case ALife::eHitTypeRadiation: return "ui_inv_outfit_radiation_protection";
        case ALife::eHitTypeTelepatic: return "ui_inv_outfit_telepatic_protection";
        default: return nullptr;
        }
    }

    // Nazwa klasy pancerza dla progu przebicia, wg [ui_armor_classes]
    // (thresholds/names). Zwraca nullptr, gdy sekcji brak lub wartosc jest
    // ponizej pierwszego progu (np. -1 = brak krytej kosci). Definicja w
    // UIActorStateInfo.cpp; progi sa parsowane i cache'owane raz.
    const char* ArmorClassName(float value);

    // Grupy kosci pancerza: tooltip klasy pancerza na panelu postaci i wiersz
    // "Pancerz" okna ulepszen. Dlonie i stopy sa swiadomie pominiete - maja
    // wszedzie wartosci szczatkowe. Twarz jest osobno, bo bywa slabsza od
    // czaszki i w jednej grupie z glowa znikalaby pod maksimum.
    //
    // "section" dzieli tabele na dwie czesci pokazywane w tooltipie osobno:
    // 0 = to, co kryje glowe (helm ALBO zintegrowany kaptur kombinezonu),
    // 1 = reszta sylwetki. Z maksimum kazdej sekcji bierze sie para liczb
    // przy pasku, wiec liczba po lewej nie zalezy od tego, czy w slocie helmu
    // cos siedzi - exoszkielet ma oslone glowy we wlasnym bones_koeff_protection.
    // label_key to podpis w tooltipie panelu, zone_key nazwa strefy w zdaniu
    // wiersza okna ulepszen ("tulow +8 pkt").
    struct ArmorGroup
    {
        int         section;
        const char* label_key;
        const char* zone_key;
        const char* bones[8];
    };

    static constexpr ArmorGroup ArmorGroups[] =
    {
        { 0, "ui_armor_tt_head",  "ui_uip_upg_zone_head",  { "bip01_head", nullptr } },
        { 0, "ui_armor_tt_face",  "ui_uip_upg_zone_face",  { "eyelid_1", "eye_left", "eye_right", "jaw_1", nullptr } },
        { 0, "ui_armor_tt_neck",  "ui_uip_upg_zone_neck",  { "bip01_neck", nullptr } },
        { 1, "ui_armor_tt_torso", "ui_uip_upg_zone_torso", { "bip01_pelvis", "bip01_spine", "bip01_spine1", "bip01_spine2",
                                                             "bip01_l_clavicle", "bip01_r_clavicle", nullptr } },
        { 1, "ui_armor_tt_arms",  "ui_uip_upg_zone_arms",  { "bip01_l_upperarm", "bip01_r_upperarm",
                                                             "bip01_l_forearm", "bip01_r_forearm", nullptr } },
        { 1, "ui_armor_tt_legs",  "ui_uip_upg_zone_legs",  { "bip01_l_thigh", "bip01_r_thigh",
                                                             "bip01_l_calf", "bip01_r_calf", nullptr } },
    };

    static constexpr unsigned ArmorGroupCount = sizeof(ArmorGroups) / sizeof(ArmorGroups[0]);

    // Liczba panelu dla progu przebicia: setne progu, zaokraglone jak przy
    // pasku "Pancerz" i w tooltipie klasy pancerza.
    inline int ArmorPoints(float armor)
    {
        return (int)std::floor(armor * 100.0f + 0.5f);
    }
}
