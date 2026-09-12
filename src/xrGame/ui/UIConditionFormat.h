#pragma once

#include "../ConditionUiValues.h"
#include "../Level.h"
#include "../../xrEngine/device.h"
#include "../../xrEngine/string_table.h"

namespace ConditionUi
{
    inline bool RadiationUnitsEnabled()
    {
        const shared_str unit = g_pStringTable->translate("ui_uip_unit_kbq");
        return unit.size() && xr_strcmp(unit.c_str(), "ui_uip_unit_kbq") != 0;
    }

    inline float CurrentTimeFactor()
    {
        return RealTimeFactor(IsGameTypeSingle(), Level().GetGameTimeFactor(), Device.time_factor());
    }

    inline char DecimalSeparator()
    {
        const shared_str separator = g_pStringTable->translate("ui_uip_decimal_sep");
        return separator.size() == 1 ? separator.c_str()[0] : '.';
    }

    inline void FormatRadiationRate(string64& out, float ratePerGameSecond)
    {
        string32 number;
        FormatNumber(number, ratePerGameSecond * CurrentTimeFactor() * RadiationScale, DecimalSeparator(), true);
        xr_strconcat(out, number, " ", g_pStringTable->translate("ui_uip_unit_kbq_s").c_str());
    }

    inline bool RegenerationUnitsEnabled()
    {
        const shared_str unit = g_pStringTable->translate("ui_uip_unit_regeneration");
        return unit.size() && xr_strcmp(unit.c_str(), "ui_uip_unit_regeneration") != 0;
    }

    inline void FormatRegenerationRate(string64& out, float ratePerGameSecond, bool showSign = true)
    {
        string32 number;
        FormatNumber(number, PercentPerSecond(ratePerGameSecond, CurrentTimeFactor()), DecimalSeparator(), showSign);
        xr_strconcat(out, number, g_pStringTable->translate("ui_uip_unit_regeneration").c_str());
    }

    inline void FormatProtectionPoints(string64& out, float ratio, bool showSign = true)
    {
        string32 number;
        FormatNumber(number, ratio * 100.0f, DecimalSeparator(), showSign);
        const shared_str unit = g_pStringTable->translate("ui_uip_unit_protection");
        const bool translated = unit.size() && xr_strcmp(unit.c_str(), "ui_uip_unit_protection") != 0;
        xr_strconcat(out, number, " ", translated ? unit.c_str() : "pt");
    }

    // Sama liczba calkowita ochrony: bez znaku, bez jednostki, 0 miejsc po
    // przecinku. Uzywa tego samego %.0f co auto_static panelu postaci
    // (decimals="0"), zeby wartosc w tooltipie i na panelu nigdy sie nie
    // rozjechala. Osobna od FormatProtectionPoints, ktora dalej sluzy
    // artefaktom (znak + jednostka "pkt").
    inline void FormatProtectionPointsPlain(string64& out, float ratio)
    {
        FormatNumber(out, ratio * 100.0f, DecimalSeparator(), false, 0);
    }

    inline u32 RadiationColor(float rate)
    {
        return rate < 0.0f ? color_rgba(110, 190, 115, 255)
            : (rate > 0.0f ? color_rgba(210, 50, 50, 255) : color_rgba(170, 170, 170, 255));
    }
}
