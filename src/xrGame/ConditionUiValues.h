#pragma once

// GameSpy headers alias snprintf to _snprintf in some Windows/unity units.
// Protect both the standard headers and our calls, then restore the caller.
#pragma push_macro("snprintf")
#undef snprintf

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ConditionUi
{
    struct RegenerationSources
    {
        float natural = 0.0f;
        float rest = 0.0f;
        float equipment = 0.0f;
        float temporary = 0.0f;

        float Total() const { return natural + rest + equipment + temporary; }
    };

    constexpr float BleedingMaximum = 100.0f;

    inline float BleedingIntensity(float bleedingSpeed)
    {
        return bleedingSpeed * 100.0f;
    }

    constexpr float HealthRegenerationMaximum = 2.5f;
    constexpr float PowerRegenerationMaximum = 20.0f;

    inline float PercentPerSecond(float rate, float timeFactor)
    {
        return rate * timeFactor * 100.0f;
    }

    // Presentation scale: the engine's 0..1 radiation resource is 0..100 kBq.
    constexpr float RadiationScale = 100.0f;

    inline float RealTimeFactor(bool singlePlayer, float worldFactor, float deviceFactor)
    {
        return singlePlayer ? worldFactor * deviceFactor : 1.0f;
    }

    template <size_t N>
    void FormatNumber(char (&out)[N], double value, char decimalSeparator, bool showSign, int decimals = 2)
    {
        const double magnitude = std::fabs(value);
        // Suppress floating-point cancellation noise, including negative zero.
        if (magnitude < 0.000001)
            std::snprintf(out, N, "0");
        else if (decimals == 2 && magnitude < 0.01)
            std::snprintf(out, N, "%s<0.01", value < 0 ? "-" : (showSign ? "+" : ""));
        else
        {
            std::snprintf(out, N, showSign ? "%+.*f" : "%.*f", decimals, value);
            if (char* dot = std::strchr(out, '.'))
            {
                char* end = out + std::strlen(out) - 1;
                while (end > dot && *end == '0')
                    *end-- = '\0';
                if (end == dot)
                    *end = '\0';
            }
        }
        if (char* dot = std::strchr(out, '.'))
            *dot = decimalSeparator;
    }

    // Actual, clamped resource changes; no duplicated list of radiation sources.
    // Publish at half-second intervals to smooth scheduled equipment/zone ticks.
    class RadiationRate
    {
    public:
        void Reset()
        {
            m_delta = 0.0;
            m_seconds = 0.0;
            m_rate = 0.0f;
        }

        void Add(float delta, float seconds)
        {
            if (seconds <= 0.0f)
                return; // Pause: do not dilute the last running measurement.
            if (!std::isfinite(delta) || !std::isfinite(seconds) || seconds > 2.0f)
            {
                Reset(); // Loading, sleep/time jumps or invalid samples.
                return;
            }
            m_delta += delta;
            m_seconds += seconds;
            if (m_seconds >= 0.5 - 0.000001)
            {
                m_rate = static_cast<float>(m_delta / m_seconds);
                m_delta = 0.0;
                m_seconds = 0.0;
            }
        }

        float Get() const { return m_rate; }

    private:
        double m_delta = 0.0;
        double m_seconds = 0.0;
        float m_rate = 0.0f;
    };
}

#pragma pop_macro("snprintf")
