#pragma once

#include <algorithm>
#include <cmath>

// Runtime options are persisted by the balance addon, not user.ltx.
extern int g_BulletApSpeedFalloff;
extern float g_BulletApSpeedCompensation;

namespace BulletArmorPenetration
{
	inline float NormalizeRetention(float Retention)
	{
		return std::isfinite(Retention) && Retention >= 1.f ? Retention : 1.f;
	}

	inline float AtSpeed(float BaseAp, float Speed, float InitialSpeed, float Compensation, float Retention = 1.f)
	{
		if (!std::isfinite(Speed) || !std::isfinite(InitialSpeed) || InitialSpeed <= 0.f || !std::isfinite(Compensation))
		{
			return BaseAp;
		}

		const float RetainedSpeed = std::clamp(Speed / InitialSpeed, 0.f, 1.f);
		const float CompensatedLoss = std::clamp(Compensation, 0.f, 1.f);
		const float EffectiveRetention = NormalizeRetention(Retention);
		if (EffectiveRetention == 1.f)
		{
			// Keep the original calculation exactly for unmodified ammunition.
			return BaseAp * (CompensatedLoss + (1.f - CompensatedLoss) * RetainedSpeed);
		}

		return BaseAp * (1.f - (1.f - CompensatedLoss) * (1.f - RetainedSpeed) / EffectiveRetention);
	}
}
