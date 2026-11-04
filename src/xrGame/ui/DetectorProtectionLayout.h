#pragma once

#include <algorithm>
#include <cmath>

namespace DetectorProtectionLayout
{
    struct Position { float X, Y; };

    inline Position PanelPosition(float ScreenWidth, float ScreenHeight, float Width, float Height,
        float HorizontalOffset, float BottomMargin)
    {
        return {std::floor((ScreenWidth - Width) * 0.5f + HorizontalOffset + 0.5f),
            std::floor(ScreenHeight - Height - BottomMargin + 0.5f)};
    }

    inline float MarkerY(float BarTop, float BarHeight, float MarkerHeight, float Source, float PixelScale)
    {
        float Offset = (1.0f - std::clamp(Source, 0.0f, 1.0f)) * BarHeight;
        if (PixelScale > 0.0f)
            Offset = std::floor(Offset * PixelScale + 0.5f) / PixelScale;
        return BarTop + Offset - MarkerHeight * 0.5f;
    }
}
