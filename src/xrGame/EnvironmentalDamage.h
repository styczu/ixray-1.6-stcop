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

    // Tempo rzeczywistej utraty zdrowia/psychiki (ulamek/s), z rozwiazanych
    // trafien. 0 gdy brak swiezego trafienia (poza strefa).
    struct DamageRate
    {
        float health = 0.0f;
        float psy = 0.0f;
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
        // Akumulator tempa: sumujemy rozwiazane obrazenia i czas miedzy trafieniami,
        // publikujemy rate = suma/czas co >=PublishMs; reset przy przerwie (poza strefa).
        struct RateAcc
        {
            double sumHealth = 0.0, sumPsy = 0.0, seconds = 0.0;
            std::uint32_t last = 0;
            float rateHealth = 0.0f, ratePsy = 0.0f;
        };
        RateAcc rates[ALife::eHitTypeMax] = {};
        static constexpr double PublishSec = 0.5;   // co ile publikowac
        static constexpr double GapSec = 2.0;       // przerwa -> reset okna
        static constexpr std::uint32_t StaleMs = 1000; // brak trafien -> tempo 0
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
            const float h = health > 0.0f ? health : 0.0f;
            const float p = psy > 0.0f ? psy : 0.0f;
            Sample& sample = samples[Channel(type)];
            sample.damage.health = h;
            sample.damage.psy = p;
            sample.damage.radiation = radiation > 0.0f ? radiation : 0.0f;
            sample.damage.valid = true;
            sample.time = now;

            RateAcc& r = rates[Channel(type)];
            if (r.last != 0)
            {
                const double dt = (now - r.last) / 1000.0;
                if (dt > 0.0 && dt <= GapSec)
                {
                    r.sumHealth += h; r.sumPsy += p; r.seconds += dt;
                    if (r.seconds >= PublishSec)
                    {
                        r.rateHealth = float(r.sumHealth / r.seconds);
                        r.ratePsy = float(r.sumPsy / r.seconds);
                        r.sumHealth = r.sumPsy = r.seconds = 0.0;
                    }
                }
                else // przerwa: nowe okno
                {
                    r.sumHealth = r.sumPsy = r.seconds = 0.0;
                }
            }
            r.last = now;
        }

        DamageRate GetRate(ALife::EHitType type, std::uint32_t now) const
        {
            if (!IsZoneType(type))
                return {};
            const RateAcc& r = rates[Channel(type)];
            if (r.last == 0 || now - r.last >= StaleMs)
                return {};
            return { r.rateHealth, r.ratePsy };
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
