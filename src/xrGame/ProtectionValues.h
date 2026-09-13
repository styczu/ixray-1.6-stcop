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

    // Presentation scale, not a percentage reduction of an arbitrary hit.
    // Keep values above 1 and below 0; only the graphical fill is clamped.
    inline float Ratio(float protection, float zoneMaxPower)
    {
        return zoneMaxPower > 0.0f ? protection / zoneMaxPower : 0.0f;
    }

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
}
