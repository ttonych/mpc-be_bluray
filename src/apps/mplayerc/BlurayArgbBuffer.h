// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <libbluray/overlay.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

// Java threads only copy pixels here. COM and GDI stay on the UI thread.
// FLUSH publishes a complete image; later DRAWs cannot alter that image.
class BlurayArgbBuffer {
public:
    struct Frame {
        unsigned width = 0, height = 0;
        int64_t pts = -1;
        std::vector<uint8_t> pixels;
        bool HasVisiblePixels() const {
            // RGB values can remain nonzero after a Java scene becomes fully
            // transparent. Only alpha determines whether it covers the video.
            for (size_t i = 3; i < pixels.size(); i += 4) {
                if (pixels[i]) return true;
            }
            return false;
        }
    };
private:
    struct Plane { Frame drawing, published; bool ready = false; };
    std::array<Plane, 3> planes;
    std::mutex mutex;
    bool failed = false;
public:
    void Apply(const BD_ARGB_OVERLAY* ov) noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        try {
            if (!ov) {
                for (auto& p : planes) { p = {}; p.ready = true; }
                return;
            }
            if (ov->plane >= planes.size()) { failed = true; return; }
            auto& p = planes[ov->plane];
            switch (ov->cmd) {
            case BD_ARGB_OVERLAY_INIT:
                if (!ov->w || !ov->h || ov->w > 4096 || ov->h > 2160 || ov->x || ov->y) {
                    failed = true; return;
                }
                p = {}; p.drawing.width = ov->w; p.drawing.height = ov->h;
                p.drawing.pixels.assign(size_t(ov->w) * ov->h * 4, 0);
                p.published = p.drawing; p.ready = true;
                break;
            case BD_ARGB_OVERLAY_CLOSE:
                p = {}; p.ready = true; break;
            case BD_ARGB_OVERLAY_DRAW:
                if (!ov->argb || ov->stride < ov->w || !p.drawing.width ||
                    unsigned(ov->x) + ov->w > p.drawing.width || unsigned(ov->y) + ov->h > p.drawing.height) {
                    failed = true; return;
                }
                for (unsigned y = 0; y < ov->h; ++y) {
                    // uint32_t AARRGGBB is BGRA in Windows little-endian memory.
                    memcpy(p.drawing.pixels.data() + (size_t(y + ov->y) * p.drawing.width + ov->x) * 4,
                        ov->argb + size_t(y) * ov->stride, size_t(ov->w) * 4);
                }
                break;
            case BD_ARGB_OVERLAY_FLUSH:
                p.published = p.drawing; p.published.pts = ov->pts; p.ready = true; break;
            default: failed = true; break;
            }
        } catch (...) { failed = true; }
    }
    bool Take(unsigned plane, Frame& frame) {
        std::lock_guard<std::mutex> lock(mutex);
        auto& p = planes.at(plane);
        if (!p.ready) return false;
        frame = std::move(p.published); p.ready = false;
        return true;
    }
    bool Failed() { std::lock_guard<std::mutex> lock(mutex); return failed; }
};
