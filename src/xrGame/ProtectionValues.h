#pragma once

#include "../xrEngine/AI/alife_space.h"

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
