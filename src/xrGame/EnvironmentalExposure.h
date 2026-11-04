#pragma once

#include "ProtectionValues.h"
#include <cmath>
#include <cstdint>

namespace Protection
{
    struct SourceColor { unsigned r, g, b; };

    inline SourceColor GetSourceColor(ALife::EHitType type)
    {
        switch (type)
        {
        case ALife::eHitTypeBurn:
        case ALife::eHitTypeLightBurn: return {255, 160, 80};
        case ALife::eHitTypeChemicalBurn: return {170, 235, 140};
        case ALife::eHitTypeShock: return {130, 210, 255};
        case ALife::eHitTypeTelepatic: return {205, 165, 250};
        case ALife::eHitTypeRadiation: return {245, 230, 120};
        default: return {224, 230, 234};
        }
    }

    struct ExposureReading
    {
        float power = 0.0f;   // szczyt z okna LifetimeMs (750 ms)
        float current = 0.0f; // biezaca sila - max z ostatnich CurrentMs (150 ms)
        float opacity = 0.0f;
    };

    // Per-actor UI observation, never used to calculate damage. Record incoming
    // hits before equipment subtraction, including hits that are fully absorbed.
    // A bounded peak history keeps short pulses visible without adding unrelated
    // hits or converting a hit's power into a damage-per-second rate.
    class ExposureHistory
    {
        static constexpr unsigned ChannelCount = 5;
        static constexpr unsigned BucketCount = 16;
        static constexpr std::uint32_t BucketMs = 50;
        static constexpr std::uint32_t HoldMs = 500;
        static constexpr std::uint32_t LifetimeMs = 750;
        static constexpr std::uint32_t CurrentMs = 150; // okno "biezacej" sily
        struct Sample
        {
            float power = 0.0f;
            std::uint32_t time = 0;
        };
        struct Bucket
        {
            std::uint32_t start = 0;
            Sample samples[ChannelCount] = {};
        };
        Bucket buckets[BucketCount] = {};
        unsigned current = 0;
        bool initialized = false;

        static unsigned Channel(ALife::EHitType type)
        {
            switch (type)
            {
            case ALife::eHitTypeBurn:
            case ALife::eHitTypeLightBurn: return 0;
            case ALife::eHitTypeShock: return 1;
            case ALife::eHitTypeChemicalBurn: return 2;
            case ALife::eHitTypeRadiation: return 3;
            case ALife::eHitTypeTelepatic: return 4;
            default: return ChannelCount;
            }
        }

    public:
        void Reset() { *this = ExposureHistory{}; }

        void Record(ALife::EHitType type, float power, std::uint32_t now)
        {
            const unsigned channel = Channel(type);
            if (channel == ChannelCount || !(power > 0.0f) || !std::isfinite(power))
                return;
            if (!initialized || now - buckets[current].start >= BucketMs)
            {
                current = initialized ? (current + 1) % BucketCount : 0;
                buckets[current] = Bucket{};
                buckets[current].start = now;
                initialized = true;
            }
            Sample& sample = buckets[current].samples[channel];
            if (power >= sample.power)
            {
                sample.power = power;
                sample.time = now;
            }
        }

        ExposureReading Get(ALife::EHitType type, std::uint32_t now) const
        {
            const unsigned channel = Channel(type);
            ExposureReading result;
            if (channel == ChannelCount || !initialized)
                return result;
            std::uint32_t youngest = LifetimeMs;
            for (const Bucket& bucket : buckets)
            {
                const Sample& sample = bucket.samples[channel];
                const std::uint32_t age = now - sample.time;
                if (age >= LifetimeMs || !(sample.power > 0.0f))
                    continue;
                if (sample.power > result.power || (sample.power == result.power && age < youngest))
                {
                    result.power = sample.power;
                    youngest = age;
                }
                if (age < CurrentMs && sample.power > result.current)
                    result.current = sample.power;
            }
            if (result.power > 0.0f)
                result.opacity = youngest <= HoldMs ? 1.0f
                    : float(LifetimeMs - youngest) / float(LifetimeMs - HoldMs);
            return result;
        }
    };
}
