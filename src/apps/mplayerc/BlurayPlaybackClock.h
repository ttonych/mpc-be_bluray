// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>

// Some graphs report audio EOS immediately after seeking into a video-only
// tail. Continue the presentation clock instead of advancing disc navigation
// to the playlist's end while madVR still has video to present.
class BlurayPlaybackClock {
    int64_t position = -1;
    uint64_t tick = 0;
    bool afterSeek = false;
    bool fallback = false;
public:
    void Reset() { *this = {}; }
    void Seek(int64_t target, uint64_t now) {
        position = target; tick = now; afterSeek = true; fallback = false;
    }
    int64_t Update(int64_t reported, uint64_t now, bool running, double rate, int64_t end) {
        const int64_t elapsed = position >= 0 && running && now >= tick
            ? int64_t(double(now - tick) * 10000.0 * rate) : 0;
        const int64_t expected = std::min(end, std::max<int64_t>(0, position + elapsed));
        if (afterSeek && position >= 0 && reported >= end && expected + 1000000 < end) {
            fallback = true;
        }
        position = fallback ? expected : reported;
        tick = now;
        return position;
    }
    bool AcceptEnd(int64_t end) {
        if (position >= 0 && position + 1000000 < end) {
            fallback = true;
            return false;
        }
        return true;
    }
    int64_t Position(int64_t reported) const { return fallback ? position : reported; }
    bool IsFallback() const { return fallback; }
};
