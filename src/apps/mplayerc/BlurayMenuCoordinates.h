// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

// rendererRect is in the player's client coordinates; activeVideoRect is in
// madVR's render-target coordinates, as used by OsdSetBitmap(relative=true).
// Keep the full active rectangle when zoom crops the picture at window edges.
inline bool MapBlurayMenuPoint(const POINT& point, const RECT& rendererRect,
    const RECT& activeVideoRect, unsigned width, unsigned height, POINT& mapped)
{
    if (!width || !height || point.x < rendererRect.left || point.x >= rendererRect.right
        || point.y < rendererRect.top || point.y >= rendererRect.bottom) return false;
    const int64_t w = int64_t(activeVideoRect.right) - activeVideoRect.left;
    const int64_t h = int64_t(activeVideoRect.bottom) - activeVideoRect.top;
    const int64_t x = int64_t(point.x) - rendererRect.left - activeVideoRect.left;
    const int64_t y = int64_t(point.y) - rendererRect.top - activeVideoRect.top;
    if (w <= 0 || h <= 0 || x < 0 || x >= w || y < 0 || y >= h) return false;
    mapped.x = LONG(x * width / w);
    mapped.y = LONG(y * height / h);
    return true;
}
