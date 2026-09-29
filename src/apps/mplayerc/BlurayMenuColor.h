// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <libbluray/bluray.h>
#include <libbluray/overlay.h>

// UHD graphics mixed with HDR video are authored in BT.2020/ST 2084
// (BD-ROM Part 3 v3.1 white paper, section 2.3.1.3). madVR's bitmap OSD
// accepts ordinary RGB, independently of the video's HDR processing.
namespace BlurayMenuColor {
    inline bool IsPq2020(const BLURAY_STREAM_INFO& video) {
        return video.color_space == BLURAY_COLOR_SPACE_BT2020 &&
            (video.dynamic_range_type == BLURAY_DYNAMIC_RANGE_HDR10 ||
             video.dynamic_range_type == BLURAY_DYNAMIC_RANGE_DOLBY_VISION);
    }

    struct Tables {
        std::array<double, 256> pq;
        std::array<uint8_t, 4097> srgb;
        Tables() {
            // Map HDR reference white (203 cd/m2, ITU-R BT.2408) to OSD white.
            // This is a UI conversion, not the tone mapper for the movie.
            constexpr double m1 = 2610.0 / 16384, m2 = 2523.0 / 32;
            constexpr double c1 = 3424.0 / 4096, c2 = 2413.0 / 128, c3 = 2392.0 / 128;
            for (size_t i = 0; i < pq.size(); ++i) {
                const double p = std::pow(double(i) / 255, 1 / m2);
                pq[i] = std::pow(std::max(p - c1, 0.0) / (c2 - c3 * p), 1 / m1) * (10000 / 203.0);
            }
            for (size_t i = 0; i < srgb.size(); ++i) {
                const double linear = double(i) / (srgb.size() - 1);
                const double encoded = linear <= 0.0031308 ? 12.92 * linear
                    : 1.055 * std::pow(linear, 1 / 2.4) - 0.055;
                srgb[i] = uint8_t(std::clamp(encoded * 255 + 0.5, 0.0, 255.0));
            }
        }
        uint8_t Encode(double linear) const {
            return srgb[size_t(std::clamp(linear, 0.0, 1.0) * (srgb.size() - 1) + 0.5)];
        }
    };

    inline void CopyToOsd(uint8_t* destination, const uint8_t* source, size_t bytes, bool pq2020) {
        if (!pq2020) {
            std::memcpy(destination, source, bytes);
            return;
        }
        static const Tables tables;
        for (size_t i = 0; i < bytes; i += 4) {
            const uint8_t alpha = source[i + 3];
            if (!alpha) {
                std::memset(destination + i, 0, 4);
                continue;
            }
            // libbluray supplies straight BGRA. Convert colour before alpha
            // blending; alpha (including antialiased edges) is not luminance.
            const double b = tables.pq[source[i]], g = tables.pq[source[i + 1]], r = tables.pq[source[i + 2]];
            // Linear BT.2020 -> BT.709/sRGB (D65), then clip to the OSD gamut.
            destination[i]     = tables.Encode(-0.018151 * r - 0.100579 * g + 1.118730 * b);
            destination[i + 1] = tables.Encode(-0.124550 * r + 1.132900 * g - 0.008350 * b);
            destination[i + 2] = tables.Encode( 1.660491 * r - 0.587641 * g - 0.072850 * b);
            destination[i + 3] = alpha;
        }
    }

    inline void CopyYcbcrToOsd(uint8_t* destination, const uint8_t* source, size_t bytes, bool pq2020) {
        uint8_t previous[4]{}, converted[4]{};
        bool havePrevious = false;
        const auto clamp = [](double v) { return uint8_t(std::clamp(v + .5, 0.0, 255.0)); };
        // libbluray/overlay.h: the palette uses the associated video's matrix.
        // Input is retained Y'CrCb/alpha from each drawn region, not one global
        // palette: a later DRAW/palette update must not recolour other regions.
        for (size_t i = 0; i < bytes; i += 4) {
            if (!havePrevious || std::memcmp(previous, source + i, 4)) {
                std::memcpy(previous, source + i, 4);
                havePrevious = true;
                if (!source[i + 3]) {
                    std::memset(converted, 0, 4);
                } else {
                    const double y = 1.164383 * (int(source[i]) - 16);
                    const double cr = int(source[i + 1]) - 128, cb = int(source[i + 2]) - 128;
                    uint8_t rgb[4];
                    if (pq2020) {
                        // ITU-R BT.2020 non-constant luminance, Kr=.2627, Kb=.0593.
                        rgb[0] = clamp(y + 2.141772 * cb);
                        rgb[1] = clamp(y - .187326 * cb - .650424 * cr);
                        rgb[2] = clamp(y + 1.678674 * cr);
                    } else {
                        // Preserve the original BT.709 path for non-HDR menus.
                        rgb[0] = clamp(y + 2.112402 * cb);
                        rgb[1] = clamp(y - .213249 * cb - .532909 * cr);
                        rgb[2] = clamp(y + 1.792741 * cr);
                    }
                    rgb[3] = source[i + 3];
                    CopyToOsd(converted, rgb, 4, pq2020);
                }
            }
            // Menus contain long flat runs; reuse the last converted colour.
            std::memcpy(destination + i, converted, 4);
        }
    }
}
