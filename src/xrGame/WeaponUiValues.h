#pragma once

#include <cmath>

// Weapon tooltip bars (ui-weapon-params). Every bar shows the weapon as it
// fires now: upgrades, wear, the loaded cartridge and the silencer. Scores are
// 0..100 on the bar; past 100 the tooltip shows the overflow triangle. Scales
// and examples for the whole STCoP set: docs/features/ui-weapon-params.md.
namespace WeaponUi
{
    constexpr float RadToDeg = 57.2957795f;

    constexpr float DamageFull = 1.5f;          // hit power of a full bar
    // Armour penetration is the cartridge's k_ap in hundredths, like the ammo tooltip
    // and the panel's "Armour" bar: 100 (k_ap 1.0) fills the bar. From 10.0 up
    // (gauss's 999) it only means "goes through everything": "max".
    constexpr float PenetrationMax = 10.0f;
    inline float PenetrationScore(float kAp)
    {
        return kAp * 100.0f;
    }
    constexpr float DispersionBest = 0.05f;     // degrees: 100
    constexpr float DispersionWorst = 3.0f;     // degrees: 0
    constexpr float RecoilBest = 0.10f;         // degrees per shot: 100
    constexpr float RecoilWorst = 5.0f;         // degrees per shot: 0
    // A recoil pattern step is about 2.28x the legacy cam_dispersion figure for
    // the same weapon class (median assault rifle 0.855 vs 0.375 degrees).
    constexpr float PatternRecoilDivisor = 2.28f;
    constexpr float MisfireBest = 0.005f;       // mean misfire chance: 100
    constexpr float MisfireWorst = 0.08f;       // mean misfire chance: 0
    constexpr float ShotsWorst = 100.0f;        // shots from 100% to 0%: 0
    constexpr float ShotsBest = 3000.0f;        // shots from 100% to 0%: 100
    constexpr float HipBest = 1.0f;             // degrees of hip fire spread walking: 100
    constexpr float HipWorst = 8.0f;            // degrees: 0
    // v / VEL_MAX of a walk, about 3 m/s (CActor::GetWeaponAccuracy, VEL_MAX 10):
    // a model of the tooltip, not measured in game.
    constexpr float WalkVelocity = 0.3f;
    constexpr float HoldWorst = 3.0f;           // fire_dispersion_condition_factor: 0

    // 100 at best, 0 at worst, logarithmic, unclamped.
    inline float LogScore(float value, float best, float worst)
    {
        return 100.0f * std::log(worst / value) / std::log(worst / best);
    }

    // Hit of one shot: weapon x cartridge x silencer, shot counted as
    // sqrt(pellets) (40 pellets ~ 6.3 bullets), so k_hit stays per pellet.
    inline float Damage(float hitPower, float kHit, float silencer, int pellets)
    {
        return hitPower * kHit * silencer * std::sqrt(float(pellets > 1 ? pellets : 1));
    }

    inline float DamageScore(float damage)
    {
        return damage * 100.0f;
    }

    // Same product as CWeapon::GetBaseDispersion (degrees), without the
    // holder's stance term that GetFireDispersion adds.
    inline float Dispersion(float baseDeg, float kDisp, float silencer, float conditionFactor, float condition)
    {
        return baseDeg * silencer * kDisp * (1.0f + conditionFactor * (1.0f - condition));
    }

    inline float AccuracyScore(float dispersionDeg)
    {
        return LogScore(dispersionDeg, DispersionBest, DispersionWorst);
    }

    inline float Clamp100(float value)
    {
        return value < 0.0f ? 0.0f : (value > 100.0f ? 100.0f : value);
    }

    // A value as the tooltip prints it, in whole steps of 10^-decimals (never
    // below zero).
    inline int ShownSteps(float value, int decimals)
    {
        float scale = 1.0f;
        for (int i = 0; i < decimals; ++i)
            scale *= 10.0f;
        return value > 0.0f ? static_cast<int>(std::floor(value * scale + 0.5f)) : 0;
    }

    // The difference of two printed values, so the number next to a value is
    // exactly what the reader gets subtracting the two figures; 0: print nothing.
    inline int ShownDelta(float value, float other, int decimals)
    {
        return ShownSteps(value, decimals) - ShownSteps(other, decimals);
    }

    // The player's hip fire spread while walking (CActor::GetWeaponAccuracy:
    // the actor's disp_base x PDM_disp_base, x (1 + v / VEL_MAX x the actor's
    // disp_vel_factor x PDM_disp_vel_factor)), degrees. Aiming uses disp_aim.
    inline float HipDispersion(float actorBaseDeg, float pdmBase, float actorVelocityFactor, float pdmVelocity)
    {
        return actorBaseDeg * pdmBase * (1.0f + WalkVelocity * actorVelocityFactor * pdmVelocity);
    }

    // Crosshair settling, the time to raise the sights and the turn slowdown
    // (control_inertion_factor divides the mouse sensitivity), 2/9 each, and
    // the hip fire spread, 1/3. No spread at all (an upgrade took
    // PDM_disp_base to 0 or below; the engine does not clamp it) reads as 100.
    inline float Handling(float crosshairInertion, float zoomRotateTime, float controlInertion, float hipDeg)
    {
        const float sights = Clamp100(100.0f * (11.9f - crosshairInertion) / 10.5f)
            + Clamp100(100.0f * (0.5f - zoomRotateTime) / 0.4f)
            + Clamp100(112.0f / controlInertion - 40.0f);
        const float hip = hipDeg > 0.0f ? Clamp100(LogScore(hipDeg, HipBest, HipWorst)) : 100.0f;
        return sights * 2.0f / 9.0f + hip / 3.0f;
    }

    // Accuracy hold: how little the spread grows with wear, linear in
    // fire_dispersion_condition_factor (the spread is x (1 + k x (1 - condition))).
    inline float HoldScore(float conditionFactor)
    {
        return 100.0f * (1.0f - conditionFactor / HoldWorst);
    }

    // Recoil pattern weapons (CWeaponShotEffector::ShotFromPattern): mean step
    // length x mean of the hip and zoom factors, radians to degrees, on the
    // legacy scale.
    inline float PatternRecoil(float meanStep, float hipFactor, float zoomFactor)
    {
        return meanStep * (hipFactor + zoomFactor) * 0.5f * RadToDeg / PatternRecoilDivisor;
    }

    // Legacy recoil (Shot2Legacy): mean of the hip and zoom kick and its
    // per-shot increase, already scaled by the silencer, radians to degrees.
    inline float LegacyRecoil(float hip, float hipInc, float zoom, float zoomInc)
    {
        return (hip + hipInc + zoom + zoomInc) * 0.25f * RadToDeg;
    }

    inline float RecoilScore(float recoilDeg)
    {
        return LogScore(recoilDeg, RecoilBest, RecoilWorst);
    }

    // Mean misfire chance over the wear still ahead: (1/c) * integral 0..c of
    // the chance CWeapon::GetConditionMisfireProbability gives at condition x
    // (none above start, linear down to end, then the end chance).
    inline float MisfireMean(float start, float end, float startProb, float endProb, float condition)
    {
        if (condition <= 0.0f)
            return endProb;
        const float span = (start == end) ? start : (start - end);
        const float slope = (endProb - startProb) / span;
        float integral = endProb * (condition < end ? condition : end);
        if (condition > end)
        {
            const float upper = condition < start ? condition : start;
            if (upper > end)
                integral += startProb * (upper - end)
                    + slope * ((start - end) * (start - end) - (start - upper) * (start - upper)) * 0.5f;
        }
        return integral / condition;
    }

    // CS-style misfire (misfire_probability + (1 - x)^3 * misfire_condition_k).
    inline float LegacyMisfireMean(float probability, float conditionK, float condition)
    {
        if (condition <= 0.0f)
            return probability + conditionK;
        const float rest = 1.0f - condition;
        return probability + conditionK * (1.0f - rest * rest * rest * rest) / (4.0f * condition);
    }

    // No misfires at all reads as "max".
    inline float ReliabilityScore(float misfireMean)
    {
        return LogScore(misfireMean, MisfireBest, MisfireWorst);
    }

    // Single shots from 100% to 0% (automatic fire wears by
    // condition_queue_shot_dec from the second shot of a burst).
    inline float ShotsPerLife(float conditionShotDec, float impair)
    {
        return 1.0f / (conditionShotDec * impair);
    }

    inline float DurabilityScore(float shots)
    {
        return 100.0f * std::log(shots / ShotsWorst) / std::log(ShotsBest / ShotsWorst);
    }

    // Effective range (model V4): the distance of 50% hits on a 0.5 x 0.8 m
    // target with the first steadied shot, drop compensated. The weapon's
    // dispersion w (degrees, above) meets the practical aiming dispersion a of
    // its platform and sight: a model for this figure, not an engine mechanic.
    inline float AimingDispersion(float platformDeg, float sightDeg)
    {
        return std::sqrt(platformDeg * platformDeg + sightDeg * sightDeg);
    }

    // Surrogate of the full simulation (the shot spread of random_dir plus the
    // aiming error): C(mix) / d with d = sqrt(w^2 + a^2).
    inline float AccuracyRange(float weaponDeg, float aimingDeg)
    {
        const float d2 = weaponDeg * weaponDeg + aimingDeg * aimingDeg;
        if (d2 <= 0.0f)
            return HUGE_VALF;
        const float mix = 2.0f * weaponDeg * aimingDeg / d2;
        return (70.3457f - 0.1438f * mix - 7.9508f * mix * mix) / std::sqrt(d2);
    }

    // The bullet's reach in the bullet manager's trajectory (air resistance A
    // takes the start velocity away linearly within 1/A s, gravity apart).
    inline float BallisticRange(float speed, float airResistance, float gravity)
    {
        if (airResistance <= 0.0f)
            return HUGE_VALF;
        const float k = gravity / (airResistance * speed);
        return speed / (2.0f * airResistance) / std::sqrt(1.0f + k * k);
    }

    inline float EffectiveRange(float accuracyRange, float ballisticRange, float maxDistance)
    {
        const float range = accuracyRange < ballisticRange ? accuracyRange : ballisticRange;
        return range < maxDistance ? range : maxDistance;
    }

    // The sight's class from its field of view in zoom (degrees, smaller is
    // closer): red dots keep about the eye's view, high power optics are narrow.
    inline const char* ScopeSight(float fov, float redDotMinFov, float opticHighMaxFov)
    {
        if (fov >= redDotMinFov)
            return "sight_red_dot";
        if (fov <= opticHighMaxFov)
            return "sight_optic_high";
        return "sight_optic_low";
    }
}
