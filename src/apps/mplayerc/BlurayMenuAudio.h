// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <libbluray/bdnav/mpls_data.h>
#include <string>
#include <vector>

struct BlurayMenuAudioClip {
    std::wstring id;
    uint32_t in = 0, out = 0; // 45 kHz, before the demuxer's timestamp origin.
};

struct BlurayMenuAudio {
    int subpath = -1, subclip = -1, pid = -1;
    bool repeat = false;
    std::vector<BlurayMenuAudioClip> clips;
};

// Primary audio in a browsable slideshow (SubPath type 2) runs independently
// of the main path's still frames. PIDs alone are not unique across M2TS files.
// Read the original STN: libbluray 1.5.0's title-info copy repeats the first
// stream's subpath_id for every stream and does not expose subclip_id.
inline bool GetBlurayMenuAudio(const MPLS_PL* playlist, unsigned item, unsigned audio,
                              BlurayMenuAudio& result)
{
    result = {};
    if (!playlist || !playlist->play_item || item >= playlist->list_count) return false;
    const auto& stn = playlist->play_item[item].stn;
    if (!stn.audio || !audio || audio > stn.num_audio) return false;
    const auto& stream = stn.audio[audio - 1];
    if ((stream.stream_type != 2 && stream.stream_type != 3)
        || !playlist->sub_path || stream.subpath_id >= playlist->sub_count) return false;
    const auto& path = playlist->sub_path[stream.subpath_id];
    if (path.type != 2 || !path.sub_play_item || !path.sub_playitem_count) return false;
    BlurayMenuAudio selected;
    selected.subpath = stream.subpath_id;
    selected.subclip = stream.stream_type == 2 ? stream.subclip_id : 0;
    selected.pid = stream.pid;
    selected.repeat = !!path.is_repeat;
    for (unsigned i = 0; i < path.sub_playitem_count; ++i) {
        const auto& entry = path.sub_play_item[i];
        if (!entry.clip || selected.subclip >= entry.clip_count || entry.out_time <= entry.in_time) return false;
        const auto& clip = entry.clip[selected.subclip];
        std::wstring id;
        for (unsigned j = 0; j < 5; ++j) {
            if (clip.clip_id[j] < '0' || clip.clip_id[j] > '9') return false;
            id += wchar_t(clip.clip_id[j]);
        }
        if (clip.clip_id[5] || std::string(clip.codec_id, 4) != "M2TS") return false;
        selected.clips.push_back({id, entry.in_time, entry.out_time});
    }
    result = std::move(selected);
    return true;
}

inline bool SameBlurayMenuAudio(const BlurayMenuAudio& a, const BlurayMenuAudio& b)
{
    return a.subpath == b.subpath && a.subclip == b.subclip && a.pid == b.pid;
}

inline bool BlurayMenuAudioRange(const BlurayMenuAudioClip& clip, int64_t origin,
                                int64_t& start, int64_t& stop)
{
    if (origin < 0 || clip.out <= clip.in) return false;
    start = int64_t(clip.in) * 2000 / 9 - origin;
    stop = int64_t(clip.out) * 2000 / 9 - origin;
    if (start < 0) start = 0;
    return stop > start;
}
