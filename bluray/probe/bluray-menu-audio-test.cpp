// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/apps/mplayerc/BlurayMenuAudio.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main()
{
    MPLS_CLIP clips[2]{};
    std::memcpy(clips[0].clip_id, "00101", 6);
    std::memcpy(clips[1].clip_id, "00102", 6);
    for (auto& c : clips) std::memcpy(c.codec_id, "M2TS", 5);
    MPLS_SUB_PI entries[2]{};
    for (auto& e : entries) {
        e.clip = clips; e.clip_count = 2;
        e.in_time = 450000; e.out_time = 495000;
    }
    MPLS_SUB paths[2]{};
    for (auto& p : paths) {
        p.type = 2; p.sub_playitem_count = 2; p.sub_play_item = entries;
    }
    paths[1].is_repeat = 1;
    MPLS_STREAM streams[3]{};
    streams[0].stream_type = 1; streams[0].pid = 0x1100;
    streams[1].stream_type = streams[2].stream_type = 2;
    streams[1].pid = streams[2].pid = 0x1100;
    streams[2].subpath_id = 1; streams[2].subclip_id = 1;
    MPLS_PI item{};
    item.stn.audio = streams; item.stn.num_audio = 3;
    MPLS_PL playlist{};
    playlist.list_count = 1; playlist.play_item = &item;
    playlist.sub_count = 2; playlist.sub_path = paths;
    BlurayMenuAudio a, b;
    assert(!GetBlurayMenuAudio(nullptr, 0, 1, a));
    assert(!GetBlurayMenuAudio(&playlist, 1, 1, a));
    assert(!GetBlurayMenuAudio(&playlist, 0, 0, a));
    assert(!GetBlurayMenuAudio(&playlist, 0, 4, a));
    // Ordinary in-mux audio must keep using the main DirectShow graph.
    assert(!GetBlurayMenuAudio(&playlist, 0, 1, a));
    assert(GetBlurayMenuAudio(&playlist, 0, 2, a));
    assert(a.pid == 0x1100 && a.subpath == 0 && !a.repeat);
    assert(a.clips.size() == 2 && a.clips[0].id == L"00101");
    assert(a.clips[1].in == 450000 && a.clips[1].out == 495000);
    // Language choices can share a PID but point to different files.
    assert(GetBlurayMenuAudio(&playlist, 0, 3, b));
    assert(b.pid == a.pid && b.subpath == 1 && b.repeat);
    assert(b.clips[0].id == L"00102" && !SameBlurayMenuAudio(a, b));
    assert(SameBlurayMenuAudio(a, a));
    b = a; b.subclip = 1; assert(!SameBlurayMenuAudio(a, b));
    b = a; b.pid++; assert(!SameBlurayMenuAudio(a, b));
    // Type 3 references the sole clip; its unused subclip byte is irrelevant.
    streams[2].stream_type = 3;
    assert(GetBlurayMenuAudio(&playlist, 0, 3, b));
    assert(b.clips[0].id == L"00101");
    streams[2].stream_type = 2;
    // Do not misroute synchronized/PiP/graphics subpaths as menu soundtracks.
    for (unsigned type : {3, 5, 6, 7, 8, 10}) {
        paths[0].type = uint8_t(type);
        assert(!GetBlurayMenuAudio(&playlist, 0, 2, a));
    }
    paths[0].type = 2;
    streams[1].subpath_id = 2; assert(!GetBlurayMenuAudio(&playlist, 0, 2, a));
    streams[1].subpath_id = 0;
    streams[1].subclip_id = 2; assert(!GetBlurayMenuAudio(&playlist, 0, 2, a));
    streams[1].subclip_id = 0;
    paths[0].sub_playitem_count = 0; assert(!GetBlurayMenuAudio(&playlist, 0, 2, a));
    paths[0].sub_playitem_count = 2;
    entries[1].out_time = entries[1].in_time;
    assert(!GetBlurayMenuAudio(&playlist, 0, 2, a) && a.clips.empty());
    entries[1].out_time = 495000;
    clips[0].clip_id[2] = '/'; assert(!GetBlurayMenuAudio(&playlist, 0, 2, a));
    clips[0].clip_id[2] = '1';
    clips[0].codec_id[0] = 'X'; assert(!GetBlurayMenuAudio(&playlist, 0, 2, a));
    clips[0].codec_id[0] = 'M';
    // Convert authored absolute PTS to the standalone demuxer's time base.
    int64_t start = 0, stop = 0;
    BlurayMenuAudioClip range{L"00101", 450000, 540000};
    assert(BlurayMenuAudioRange(range, 90000000, start, stop));
    assert(start == 10000000 && stop == 30000000);
    assert(BlurayMenuAudioRange(range, 100100000, start, stop));
    assert(start == 0 && stop == 19900000);
    assert(!BlurayMenuAudioRange(range, 120000000, start, stop));
    assert(!BlurayMenuAudioRange(range, -1, start, stop));
    range.out = UINT32_MAX;
    assert(BlurayMenuAudioRange(range, 0, start, stop));
    assert(stop == int64_t(UINT32_MAX) * 2000 / 9);
    range.out = range.in;
    assert(!BlurayMenuAudioRange(range, 0, start, stop));
    puts("Menu audio: separate PIDs/subclips, type restrictions, malformed metadata and PTS ranges passed.");
}
