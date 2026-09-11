#pragma once

#include "ProtectionValues.h"
#include <cmath>
#include <cstdint>

namespace Protection
{
    struct DamageReading
    {
        float health = 0.0f;
        float psy = 0.0f;
        float radiation = 0.0f;
        bool valid = false;
    };

    // Last resolved hit, not an estimate from equipment or a net regeneration
    // rate. Keep zero-damage hits too, so full absorption replaces an older hit.
    class DamageHistory
    {
        struct Sample
        {
            DamageReading damage;
            std::uint32_t time = 0;
        };
        Sample samples[ALife::eHitTypeMax] = {};
        static ALife::EHitType Channel(ALife::EHitType type)
        {
            return type == ALife::eHitTypeLightBurn ? ALife::eHitTypeBurn : type;
        }
    public:
        void Reset() { *this = DamageHistory{}; }
        void Record(ALife::EHitType type, float health, float psy, float radiation, std::uint32_t now)
        {
            if (!IsZoneType(type) || !std::isfinite(health) || !std::isfinite(psy) || !std::isfinite(radiation))
                return;
            Sample& sample = samples[Channel(type)];
            sample.damage.health = health > 0.0f ? health : 0.0f;
            sample.damage.psy = psy > 0.0f ? psy : 0.0f;
            sample.damage.radiation = radiation > 0.0f ? radiation : 0.0f;
            sample.damage.valid = true;
            sample.time = now;
        }
        DamageReading Get(ALife::EHitType type, std::uint32_t now) const
        {
            if (!IsZoneType(type))
                return {};
            const Sample& sample = samples[Channel(type)];
            // Engine time pauses together with the inventory/game.
            return now - sample.time < 3000 ? sample.damage : DamageReading{};
        }
    };
}
