#pragma once

#include <algorithm>
#include <cmath>

// Runtime options are persisted by the balance addon, not user.ltx.
extern int g_BulletApSpeedFalloff;
extern float g_BulletApSpeedCompensation;

namespace BulletArmorPenetration
{
	inline float AtSpeed(float BaseAp, float Speed, float InitialSpeed, float Compensation)
	{
		if (!std::isfinite(Speed) || !std::isfinite(InitialSpeed) || InitialSpeed <= 0.f || !std::isfinite(Compensation))
		{
			return BaseAp;
		}

		const float RetainedSpeed = std::clamp(Speed / InitialSpeed, 0.f, 1.f);
		const float CompensatedLoss = std::clamp(Compensation, 0.f, 1.f);
		return BaseAp * (CompensatedLoss + (1.f - CompensatedLoss) * RetainedSpeed);
	}
}
