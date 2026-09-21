// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstring>
#include <libbluray/bluray.h>

// A conservative fallback for BD-J menus/screensavers made from a short video
// loop. BD_EVENT_MENU describes the Java graphics window, not whether its
// pixels are currently visible. There is no screensaver flag in that API.
// Do not classify by playlist number, chapter count or total running time.
inline bool IsBlurayMenuBackground(const BLURAY_TITLE_INFO* title)
{
    if (!title || !title->clips || title->clip_count < 8) return false;
    auto same = [](const BLURAY_CLIP_INFO& a, const BLURAY_CLIP_INFO& b) {
        return !std::memcmp(a.clip_id, b.clip_id, sizeof(a.clip_id))
            && a.in_time == b.in_time && a.out_time == b.out_time;
    };
    // A menu may have a short opening animation before the repeating clip.
    const unsigned first = same(title->clips[0], title->clips[1]) ? 0 : 1;
    if (title->clip_count - first < 8) return false;
    const auto& loop = title->clips[first];
    for (unsigned i = 0; i < title->clip_count; ++i) {
        const auto& clip = title->clips[i];
        if (!clip.video_stream_count || clip.still_mode || clip.out_time <= clip.in_time
            || clip.out_time - clip.in_time > 120 * 90000ULL) return false;
        if (i >= first && !same(clip, loop)) return false;
    }
    return true;
}
