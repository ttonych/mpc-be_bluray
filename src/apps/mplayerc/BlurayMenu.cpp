// SPDX-License-Identifier: GPL-3.0-or-later
#include "stdafx.h"
#include "BlurayMenu.h"
#include "BlurayMenuCoordinates.h"
#include "BlurayArgbBuffer.h"
#include "BlurayMediaMonitor.h"
#include "BlurayPlaybackClock.h"
#include "BlurayMenuColor.h"
#include "BlurayMenuRle.h"
#include "BlurayMenuBackground.h"
#include "BlurayMenuAudioPlayer.h"
#include "BluraySettings.h"
#include "BlurayDiscStorage.h"
#include "BlurayCatalog.h"
#include "DSUtil/Profile.h"
#include <libbluray/bluray.h>
#include <libbluray/overlay.h>
#include <libbluray/keys.h>
#include <libbluray/player_settings.h>
#include <d3d9.h>
#include <mvrInterfaces.h>
#include <array>
#include <share.h>

#define BD_FUNCTIONS(X) \
    X(bd_get_version) X(bd_open) X(bd_close) X(bd_get_disc_info) \
    X(bd_get_event) X(bd_register_overlay_proc) X(bd_play) X(bd_read_ext) \
    X(bd_set_player_setting_str) X(bd_get_playlist_info) X(bd_free_title_info) \
    X(bd_read_mpls) X(bd_free_mpls) \
    X(bd_tell_time) X(bd_set_scr) X(bd_user_input) X(bd_mouse_select) \
    X(bd_read_skip_still) X(bd_seek_time) X(bd_mouse_select_page) \
    X(bd_init) X(bd_open_disc) X(bd_set_player_setting) X(bd_register_argb_overlay_proc)

struct CBlurayMenu::State
{
    HMODULE dll = nullptr;
    BLURAY* bd = nullptr;
    BLURAY_TITLE_INFO* title = nullptr;
#define DECLARE_BD(name) decltype(&::name) name = nullptr;
    BD_FUNCTIONS(DECLARE_BD)
#undef DECLARE_BD
    CComPtr<IMadVROsdServices> osd;
    CComPtr<IMadVRCommand> command;
    CStringW root, directory, error, pendingPlaylist, notice, discName, storageKey, storagePath;
    FILE* log = nullptr;
    ULONGLONG started = GetTickCount64();
    bool waitingGraph = false, completed = false, readEnd = false, menu = false;
    bool seeking = false, still = false;
    bool popupAvailable = false, topMenuAvailable = false;
    bool bdjActive = false, hasBdj = false, firstPlayPending = false;
    bool bdjGraphicsVisible = false;
    bool menuBackground = false;
    bool bdjPq2020 = false;
    int playbackRequest = -1;
    BlurayArgbBuffer argb;
    BlurayMediaMonitor media;
    BlurayPlaybackClock clock;
    bool readFailed = false;
    uint32_t uoMask = 0;
    bool legacyUoPolicy = true;
    unsigned uoLevel = 5;
    bool Restricted(uint32_t mask, unsigned minimum) const { return (uoMask & mask) && (legacyUoPolicy || uoLevel >= minimum); }
    unsigned playitem = 0, audioStream = 255, pgStream = 4095;
    bool pgEnabled = false, streamsDirty = false;
    int lastAudioPid = -2, lastSubtitlePid = -2;
    MPLS_PL* audioPlaylist = nullptr;
    BlurayMenuAudioPlayer menuAudio;
    UINT stillSeconds = 0;
    ULONGLONG stillUntil = 0;
    REFERENCE_TIME lastPosition = -1, pendingSeek = -1;
    REFERENCE_TIME segmentStop = 0;
    REFERENCE_TIME playerSeekPosition = 0;
    ULONGLONG playerSeekAfter = 0;
    int64_t pts = -1;
    unsigned playlist = 0;
    unsigned redrawsPending = 0;
    bool presenting = false;
    ULONGLONG redrawAfter = 0;
    CRect lastMouseRendererRect, lastMouseVideoRect;
    struct Plane {
        UINT width = 0, height = 0;
        std::vector<uint8_t> indices, pixels;
        std::array<BD_PG_PALETTE_ENTRY, 256> palette{};
        HBITMAP bitmap = nullptr;
        bool dirty = false;
        int64_t flushPts = -1;
    };
    std::array<Plane, 3> planes;

    bool MenuVisible() const {
        // BD-J's MENU event follows the lifetime of its graphics window. A
        // disc can keep that window open but clear its pixels during the film.
        return menu && (!bdjActive || bdjGraphicsVisible);
    }
    bool MenuActive() const {
        return MenuVisible() || (bdjActive && menu && menuBackground);
    }

    void Log(const char* name, int64_t a = 0, int64_t b = 0) {
        if (log) {
            fprintf(log, "%llu %s %lld %lld\n", GetTickCount64() - started, name, a, b);
            fflush(log);
        }
    }
    void QueuePlayerSeek(REFERENCE_TIME position) {
        // The DirectShow graph has already sought. Coalesce key autorepeat
        // before seeking/reading the separate navigation stream on the UI thread.
        lastPosition = position;
        clock.Seek(position, GetTickCount64());
        playerSeekPosition = position;
        playerSeekAfter = GetTickCount64() + 150;
        completed = readEnd = still = false;
        stillUntil = 0;
        Log("player_seek_queued", position);
    }
    void SyncPlayerSeek() {
        if (!playerSeekAfter || !title || waitingGraph) return;
        playerSeekAfter = 0;
        const bool graphCompleted = completed;
        seeking = true;
        const uint64_t target = std::min(uint64_t(playerSeekPosition * 9 / 1000),
            title->duration ? title->duration - 1 : uint64_t(0));
        bd_seek_time(bd, target);
        Drain();
        seeking = false;
        completed = graphCompleted;
        readEnd = still = false;
        pts = Pts(lastPosition);
        bd_set_scr(bd, pts);
        Log("player_seek", playerSeekPosition);
    }
    REFERENCE_TIME SegmentEnd() const { return segmentStop ? segmentStop : title ? REFERENCE_TIME(title->duration * 1000 / 9) : 0; }
    void Detach() {
        if (osd) {
            for (UINT i = 0; i < planes.size(); ++i) {
                CStringA name; name.Format("MPCBE.Bluray.%u", i);
                osd->OsdSetBitmap(name);
            }
        }
        osd.Release(); command.Release(); redrawsPending = 0;
    }
    ~State() {
        menuAudio.Reset();
        if (audioPlaylist) bd_free_mpls(audioPlaylist);
        media.Stop();
        Detach();
        if (title) bd_free_title_info(title);
        if (bd) bd_close(bd);
        for (auto& p : planes) if (p.bitmap) DeleteObject(p.bitmap);
        if (dll) FreeLibrary(dll);
        Log("close");
        if (log) fclose(log);
    }
    void Paint(Plane& p, size_t at, UINT index) {
        const auto& c = p.palette[index];
        const double y = 1.164383 * (int(c.Y) - 16);
        const double cb = int(c.Cb) - 128, cr = int(c.Cr) - 128;
        auto clamp = [](double v) { return uint8_t(std::clamp(v + .5, 0.0, 255.0)); };
        // HDMV prototype: HD BT.709; SD and UHD matrices need separate validation.
        p.indices[at] = uint8_t(index);
        auto dst = &p.pixels[at * 4];
        dst[0] = clamp(y + 2.112402 * cb);
        dst[1] = clamp(y - .213249 * cb - .532909 * cr);
        dst[2] = clamp(y + 1.792741 * cr);
        dst[3] = index == 255 ? 0 : c.T;
    }
    static void Overlay(void* context, const BD_OVERLAY* ov) {
        static_cast<State*>(context)->OnOverlay(ov);
    }
    static void ArgbOverlay(void* context, const BD_ARGB_OVERLAY* ov) {
        static_cast<State*>(context)->argb.Apply(ov);
    }
    void ConsumeArgb() {
        if (argb.Failed()) { error = ResStr(IDS_BD_INVALID_BDJ_GRAPHICS); return; }
        for (unsigned i = 1; i < planes.size(); ++i) {
            BlurayArgbBuffer::Frame frame;
            if (!argb.Take(i, frame)) continue;
            if (i == BD_OVERLAY_IG) {
                const bool visible = frame.HasVisiblePixels();
                if (visible != bdjGraphicsVisible) Log("bdj_graphics_visible", visible);
                bdjGraphicsVisible = visible;
            }
            auto& p = planes[i];
            p.width = frame.width; p.height = frame.height;
            p.pixels = std::move(frame.pixels); p.indices.clear();
            p.dirty = true; p.flushPts = frame.pts;
            Log("argb_frame", i, (int64_t(p.width) << 32) | p.height);
        }
    }
    void OnOverlay(const BD_OVERLAY* ov) {
        if (!ov) {
            for (auto& p : planes) { p.width = p.height = 0; p.dirty = true; }
            return;
        }
        if (ov->plane >= planes.size()) return;
        // PG subtitles are already decoded by MPC-BE; do not render them twice.
        if (ov->plane == BD_OVERLAY_PG) return;
        auto& p = planes[ov->plane];
        if (ov->cmd == BD_OVERLAY_CLOSE) {
            p.width = p.height = 0; p.dirty = true; p.flushPts = -1;
            return;
        }
        if (ov->cmd == BD_OVERLAY_INIT) {
            if (!ov->w || !ov->h || ov->w > 4096 || ov->h > 2160 || ov->x || ov->y) {
                error = ResStr(IDS_BD_UNSUPPORTED_OVERLAY_SIZE); return;
            }
            p.width = ov->w; p.height = ov->h;
            p.indices.assign(size_t(p.width) * p.height, 255);
            p.pixels.assign(p.indices.size() * 4, 0);
            p.palette = {};
            p.dirty = true; p.flushPts = -1;
            Log("overlay_init", p.width, p.height);
            return;
        }
        if (!p.width) return;
        if (ov->cmd == BD_OVERLAY_FLUSH) {
            p.dirty = true; p.flushPts = ov->pts;
            Log("overlay_flush", ov->plane, ov->pts); return;
        }
        if (ov->cmd == BD_OVERLAY_CLEAR || ov->cmd == BD_OVERLAY_HIDE) {
            std::fill(p.indices.begin(), p.indices.end(), 255);
            std::fill(p.pixels.begin(), p.pixels.end(), 0);
            return;
        }
        if (UINT(ov->x) + ov->w > p.width || UINT(ov->y) + ov->h > p.height) {
            error = ResStr(IDS_BD_INVALID_OVERLAY_REGION); return;
        }
        if (ov->cmd == BD_OVERLAY_WIPE) {
            for (UINT y = ov->y; y < UINT(ov->y) + ov->h; ++y) {
                const auto at = size_t(y) * p.width + ov->x;
                std::fill_n(p.indices.data() + at, ov->w, uint8_t(255));
                std::fill_n(p.pixels.data() + at * 4, size_t(ov->w) * 4, uint8_t(0));
            }
            return;
        }
        if (ov->cmd != BD_OVERLAY_DRAW) return;
        if (ov->palette) std::copy_n(ov->palette, 256, p.palette.begin());
        if (!ov->img) {
            for (UINT y = 0; y < ov->h; ++y) for (UINT x = 0; x < ov->w; ++x) {
                auto at = size_t(ov->y + y) * p.width + ov->x + x;
                Paint(p, at, p.indices[at]);
            }
            return;
        }
        if (!BlurayMenuRle::Decode(ov->img, ov->w, ov->h,
            [&](unsigned x, unsigned y, unsigned length, unsigned color) {
                for (unsigned j = 0; j < length; ++j)
                    Paint(p, size_t(ov->y + y) * p.width + ov->x + x + j, color);
            })) {
            Log("invalid_overlay_rle", ov->w, ov->h);
            error = ResStr(IDS_BD_INVALID_OVERLAY_RLE);
        }
    }
    void Present() {
        if (!osd || presenting || waitingGraph) return;
        // madVR can dispatch window messages inside OsdSetBitmap. A nested
        // navigation timer must not replace its graph while the call is active.
        presenting = true;
        struct ResetPresenting { bool& flag; ~ResetPresenting() { flag = false; } } reset{presenting};
        CComPtr<IMadVROsdServices> currentOsd = osd;
        bool pq2020 = false;
        if (bdjActive && title && playitem < title->clip_count) {
            const auto& clip = title->clips[playitem];
            if (clip.video_stream_count) pq2020 = BlurayMenuColor::IsPq2020(clip.video_streams[0]);
        }
        if (pq2020 != bdjPq2020) {
            bdjPq2020 = pq2020;
            Log("bdj_hdr_graphics", bdjPq2020, playlist);
            // Retain original pixels so an HDR/SDR clip change also updates a
            // static menu, without another Java FLUSH or repeated conversion.
            for (auto& p : planes) if (p.indices.empty()) p.dirty = true;
        }
        bool redraw = false;
        for (UINT i = 1; i < planes.size(); ++i) {
            auto& p = planes[i];
            if (!p.dirty || (p.flushPts >= 0 && pts >= 0 && p.flushPts > pts)) continue;
            CStringA name; name.Format("MPCBE.Bluray.%u", i);
            HBITMAP bitmap = nullptr;
            if (p.width) {
                BITMAPINFO bi{};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = p.width;
                bi.bmiHeader.biHeight = -int(p.height);
                bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
                void* bits = nullptr;
                bitmap = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (!bitmap) { error = ResStr(IDS_BD_MENU_BITMAP_ALLOC_FAILED); return; }
                BlurayMenuColor::CopyToOsd(static_cast<uint8_t*>(bits), p.pixels.data(),
                    p.pixels.size(), bdjPq2020 && p.indices.empty());
            }
            const HRESULT hr = currentOsd->OsdSetBitmap(name, bitmap, nullptr, 0, 0, 0, true,
                i == BD_OVERLAY_IG ? 20 : 10, 0, BITMAP_STRETCH_TO_OUTPUT | BITMAP_USER_INTERFACE);
            if (currentOsd != osd) {
                if (bitmap) DeleteObject(bitmap);
                return;
            }
            Log("madvr_bitmap", i, hr);
            if (FAILED(hr)) {
                if (bitmap) DeleteObject(bitmap);
                error = ResStr(IDS_BD_MENU_BITMAP_REJECTED); return;
            }
            if (p.bitmap) DeleteObject(p.bitmap);
            p.bitmap = bitmap; p.dirty = false; redraw = true;
        }
        if (redraw) {
            // madVR uploads the bitmap asynchronously. At EOS the first redraw
            // can still contain its previous texture; schedule a bounded retry
            // on later UI timer ticks so a second user action is not required.
            redrawsPending = 3;
            redrawAfter = 0;
        }
        if (redrawsPending && command && GetTickCount64() >= redrawAfter) {
            command->SendCommand("redraw");
            --redrawsPending;
            redrawAfter = GetTickCount64() + 50;
        }
    }
    void Event(const BD_EVENT& e) {
        if (!e.event) return;
        Log("event", e.event, e.param);
        switch (e.event) {
        case BD_EVENT_TITLE: {
            const auto info = bd_get_disc_info(bd);
            const BLURAY_TITLE* current = info ? (e.param == BLURAY_TITLE_FIRST_PLAY ? info->first_play
                : e.param == 0 ? info->top_menu : e.param <= info->num_titles ? info->titles[e.param] : nullptr) : nullptr;
            bdjActive = current && current->bdj;
            Log("title_mode", e.param, bdjActive);
            break;
        }
        case BD_EVENT_PLAYLIST:
            // Release the menu's audio device before the film graph is built.
            menuAudio.Reset();
            if (audioPlaylist) bd_free_mpls(audioPlaylist);
            audioPlaylist = nullptr;
            clock.Reset();
            playitem = 0;
            segmentStop = 0;
            playerSeekAfter = 0;
            playlist = e.param;
            pendingPlaylist.Format(L"%s\\BDMV\\PLAYLIST\\%05u.mpls", root.GetString(), playlist);
            waitingGraph = true; completed = readEnd = still = false;
            stillUntil = 0; lastPosition = -1; pendingSeek = -1;
            if (title) bd_free_title_info(title);
            title = bd_get_playlist_info(bd, playlist, 0);
            audioPlaylist = bd_read_mpls(CW2A(pendingPlaylist, CP_UTF8));
            menuBackground = IsBlurayMenuBackground(title);
            Log("playlist", playlist, title ? title->duration : 0);
            Log("menu_background", menuBackground, title ? title->chapter_count : 0);
            break;
        case BD_EVENT_PLAYLIST_STOP:
            menuAudio.Reset();
            if (bdjActive) { playbackRequest = 1; readEnd = true; }
            break;
        case BD_EVENT_STILL:
            if (bdjActive) playbackRequest = e.param ? 1 : 0;
            break;
        case BD_EVENT_END_OF_TITLE: readEnd = true; break;
        case BD_EVENT_MENU: menu = !!e.param; break;
        case BD_EVENT_POPUP: popupAvailable = !!e.param; break;
        case BD_EVENT_UO_MASK_CHANGED: uoMask = e.param; break;
        case BD_EVENT_PLAYITEM: playitem = e.param; streamsDirty = true; break;
        case BD_EVENT_AUDIO_STREAM: audioStream = e.param; streamsDirty = true; break;
        case BD_EVENT_PG_TEXTST_STREAM: pgStream = e.param; streamsDirty = true; break;
        case BD_EVENT_PG_TEXTST: pgEnabled = !!e.param; streamsDirty = true; break;
        case BD_EVENT_SEEK:
            playerSeekAfter = 0;
            if (!seeking) pendingSeek = REFERENCE_TIME(e.param) * 10000000 / 45000;
            readEnd = completed = still = false; stillUntil = 0;
            break;
        case BD_EVENT_STILL_TIME:
            still = true; stillSeconds = e.param;
            break;
        case BD_EVENT_READ_ERROR:
            readFailed = true;
            break;
        case BD_EVENT_ERROR:
        case BD_EVENT_ENCRYPTED:
            error.Format(ResStr(IDS_BD_NAVIGATION_FAILED), e.event, e.param);
            break;
        }
    }
    bool RunInputCommands() {
        // Execute navigation without advancing the old playlist past EOS.
        for (int i = 0; i < 128; ++i) {
            if (readFailed || media.LossReason()) return false;
            BD_EVENT e{};
            if (bd_read_ext(bd, nullptr, 0, &e) < 0) {
                readFailed = true;
                Log("input_error");
                return false;
            }
            Event(e);
            if (!error.IsEmpty()) return false;
            if (!e.event) return true;
        }
        return false;
    }
    void Drain() {
        BD_EVENT e{};
        while (bd_get_event(bd, &e)) Event(e);
    }
    int64_t Pts(REFERENCE_TIME position) {
        const int64_t time = position * 9 / 1000;
        if (still && title && playitem < title->clip_count) {
            // A renderer's clock may overshoot a two-frame segment before the
            // UI pauses it. Keep the disc clock on the held picture's clip.
            const auto& clip = title->clips[playitem];
            return int64_t(clip.out_time) - 1;
        }
        if (title && title->clip_count) {
            unsigned i = 0;
            while (i + 1 < title->clip_count && title->clips[i + 1].start_time <= uint64_t(time)) ++i;
            return time - title->clips[i].start_time + title->clips[i].in_time;
        }
        return -1;
    }
    bool FirstPlay() {
        if (!bd_play(bd)) { error = ResStr(IDS_BD_FIRST_PLAY_FAILED); return false; }
        Log("first_play");
        return true;
    }
    void Pump(REFERENCE_TIME position) {
        if (!bd || readFailed || media.LossReason() || !error.IsEmpty() || !pendingPlaylist.IsEmpty()) return;
        if (firstPlayPending) {
            // Start on a UI timer tick after the canvas graph has opened, not
            // from AttachRenderer while OnFilePostOpenMedia is still running.
            // Otherwise short-lived Java loading graphics can be drawn and
            // cleared while the UI is blocked building its first renderer.
            if (!osd || waitingGraph) return;
            firstPlayPending = false;
            Log("bdj_startup_ready");
            if (!FirstPlay()) return;
        }
        ConsumeArgb();
        if (waitingGraph && title) return;
        pts = Pts(position);
        bd_set_scr(bd, pts);
        Drain();
        if (still && completed) {
            if (!stillSeconds) { Present(); return; }
            if (!stillUntil) {
                stillUntil = GetTickCount64() + stillSeconds * 1000;
                Log("still_hold", playitem, stillSeconds);
            }
            if (GetTickCount64() < stillUntil) { Present(); return; }
            const unsigned next = playitem + 1;
            Log("still_release", playitem, stillSeconds);
            bd_read_skip_still(bd); still = false; stillUntil = 0;
            Drain();
            if (pendingPlaylist.IsEmpty() && title && next < title->clip_count) {
                pendingSeek = REFERENCE_TIME(title->clips[next].start_time * 1000 / 9);
                completed = readEnd = false;
                Present();
                return; // Configure the next graph segment before reading it.
            }
        }
        uint8_t buffer[6144];
        const auto deadline = GetTickCount64() + 12;
        for (UINT i = 0; i < 1024 && GetTickCount64() < deadline; ++i) {
            if (readFailed || media.LossReason() || !pendingPlaylist.IsEmpty() || !error.IsEmpty() || still || (readEnd && !completed)) break;
            if (title && !completed && bd_tell_time(bd) > uint64_t(position * 9 / 1000 + 9000)) break;
            BD_EVENT e{};
            const int n = bd_read_ext(bd, buffer, sizeof(buffer), &e);
            Event(e); Drain();
            if (n < 0) { readFailed = true; Log("read_failed", n); break; }
            // Java chooses its next playlist asynchronously after EOS. Do not
            // spin on the same END_OF_TITLE while its event thread is working.
            if (e.event == BD_EVENT_IDLE || e.event == BD_EVENT_END_OF_TITLE) break;
            if (!n && !e.event) break;
        }
        Present();
    }
};

CBlurayMenu::CBlurayMenu() : m(std::make_unique<State>()) {}
CBlurayMenu::~CBlurayMenu() = default;

bool CBlurayMenu::Start(const CStringW& root, CStringW& error)
{
    m->root = root;
    WCHAR exe[32768]{}; GetModuleFileNameW(nullptr, exe, _countof(exe));
    CStringW directory(exe); directory.Truncate(directory.ReverseFind(L'\\') + 1);
    m->directory = directory;
    m->log = _wfsopen(directory + L"bluray-menu.log", L"w", _SH_DENYNO);
    m->dll = LoadLibraryExW(directory + L"bluray-4.dll", nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!m->dll) { error = ResStr(IDS_BD_LIBRARY_MISSING); return false; }
#define LOAD_BD(name) m->name = reinterpret_cast<decltype(m->name)>(GetProcAddress(m->dll, #name)); \
    if (!m->name) { error = ResStr(IDS_BD_LIBRARY_INCOMPLETE); return false; }
    BD_FUNCTIONS(LOAD_BD)
#undef LOAD_BD
    int major, minor, micro;
    m->bd_get_version(&major, &minor, &micro);
    if (major != 1 || minor != 5 || micro != 0) { error = ResStr(IDS_BD_LIBRARY_VERSION_REQUIRED); return false; }
    m->bd = m->bd_init();
    if (!m->bd) { error = ResStr(IDS_BD_OPEN_FAILED); return false; }
    BluraySettings settings; settings.Load();
    if (!settings.javaHome.IsEmpty()) {
        m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_JAVA_HOME, CW2A(settings.javaHome, CP_UTF8));
    } else if (PathFileExistsW(directory + L"java\\bin\\server\\jvm.dll")) {
        m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_JAVA_HOME, CW2A(directory + L"java", CP_UTF8));
    }
    const auto profile=settings.advanced[BlurayAdvanced::Profile];
    m->bd_set_player_setting(m->bd, BLURAY_PLAYER_SETTING_PLAYER_PROFILE, profile.enabled?profile.number:BLURAY_PLAYER_PROFILE_1_v1_0);
    m->bd_set_player_setting(m->bd, BLURAY_PLAYER_SETTING_REGION_CODE, settings.region);
    if (!settings.country.IsEmpty()) {
        m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_SETTING_COUNTRY_CODE, CW2A(settings.country, CP_UTF8));
    }
    if (!settings.menuLanguage.IsEmpty()) m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_SETTING_MENU_LANG, CW2A(settings.menuLanguage, CP_UTF8));
    if (!settings.audioLanguage.IsEmpty()) m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_SETTING_AUDIO_LANG, CW2A(settings.audioLanguage, CP_UTF8));
    if (!settings.subtitleLanguage.IsEmpty()) m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_SETTING_PG_LANG, CW2A(settings.subtitleLanguage, CP_UTF8));
    m->bd_set_player_setting(m->bd, BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE, settings.persistent);
    m->Log("player_region", settings.region);
    m->Log("persistent_storage", settings.persistent);
    if (!m->bd_open_disc(m->bd, CW2A(root, CP_UTF8), nullptr)) { error = ResStr(IDS_BD_OPEN_FAILED); return false; }
    auto info = m->bd_get_disc_info(m->bd);
    // Disc opening may initialize 3D/UHD registers. Explicit preferences win;
    // Auto leaves the existing per-disc initialization untouched.
    for (size_t i=0;i<settings.advanced.size();++i) if (settings.advanced[i].enabled) {
        const auto& v=settings.advanced[i]; const auto id=BlurayAdvanced::Specs[i].nativeId;
        if (!m->bd_set_player_setting(m->bd,id,v.number)) { error=ResStr(IDS_BD_ADVANCED_SETTING_REJECTED); return false; }
        m->Log("player_setting",id,v.number);
    }
    m->legacyUoPolicy=!settings.advanced[BlurayAdvanced::Restrictions].enabled;
    m->uoLevel=settings.advanced[BlurayAdvanced::Restrictions].number;
    if (info && info->num_bdj_titles && !info->bdj_handled) {
        error = ResStr(IDS_BD_JAVA_UNAVAILABLE); return false;
    }
    if (!info || !info->bluray_detected || !info->first_play_supported) {
        error = ResStr(IDS_BD_FIRST_PLAY_UNSUPPORTED); return false;
    }
    m->hasBdj = info->num_bdj_titles != 0;
    if (m->hasBdj) m->storageKey = BlurayDiscStorage::Key(root.GetString(), info->bdj_org_id, info->bdj_disc_id).c_str();
    std::filesystem::path persistent, cache;
    if (!BlurayDiscStorage::Resolve(DataDirectory().GetString(), m->storageKey.GetString(), persistent, cache,
        settings.persistentRoot.GetString(), settings.cacheRoot.GetString())) {
        error = ResStr(IDS_BD_STORAGE_CONFIG_ERROR); return false;
    }
    m->storagePath = persistent.c_str();
    m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_PERSISTENT_ROOT, CW2A(persistent.c_str(), CP_UTF8));
    m->bd_set_player_setting_str(m->bd, BLURAY_PLAYER_CACHE_ROOT, CW2A(cache.c_str(), CP_UTF8));
    if (info->disc_name) m->discName = CA2W(info->disc_name, CP_UTF8);
    m->discName.Trim();
    if (m->hasBdj && settings.persistent && !m->storageKey.IsEmpty()) {
        if (!BlurayCatalog::Remember(DataDirectory().GetString(),m->storageKey.GetString(),m->discName.GetString(),root.GetString(),persistent)) m->Log("catalog_write_failed");
    }
    if (!m->media.Start(root.GetString())) { error = ResStr(IDS_BD_DISC_UNAVAILABLE); return false; }
    m->topMenuAvailable = !!info->top_menu_supported;
    m->bd_get_event(m->bd, nullptr);
    m->bd_register_overlay_proc(m->bd, m.get(), State::Overlay);
    if (m->hasBdj) m->bd_register_argb_overlay_proc(m->bd, m.get(), State::ArgbOverlay, nullptr);
    if (m->hasBdj) {
        // Prepare the graphics-only surface before starting any disc code.
        // Java may publish a loader or language selector before selecting video.
        const CStringW canvas = directory + L"bdj-canvas.mkv";
        if (!PathFileExistsW(canvas)) { error = ResStr(IDS_BD_CANVAS_MISSING); return false; }
        m->pendingPlaylist = canvas;
        m->firstPlayPending = m->waitingGraph = true;
        m->Log("bdj_canvas");
    } else if (!m->FirstPlay()) { error = m->error; return false; }
    m->Pump(0);
    error = m->error;
    return error.IsEmpty();
}

bool CBlurayMenu::AttachRenderer(IUnknown* renderer)
{
    m->Detach();
    if (renderer) {
        renderer->QueryInterface(IID_PPV_ARGS(&m->osd));
        renderer->QueryInterface(IID_PPV_ARGS(&m->command));
    }
    m->Log("madvr_attached", m->osd != nullptr);
    if (!m->osd) { m->error = ResStr(IDS_BD_RENDERER_UNSUPPORTED); return false; }
    m->waitingGraph = false;
    m->playerSeekAfter = 0;
    m->lastPosition = -1; // A new graph starts its own playlist clock.
    m->clock.Reset();
    m->lastAudioPid = m->lastSubtitlePid = -2;
    m->streamsDirty = true;
    for (auto& p : m->planes) p.dirty = true;
    m->Present();
    return true;
}
void CBlurayMenu::DetachRenderer() { m->Detach(); }
bool CBlurayMenu::RendererBusy() const { return m->presenting; }
REFERENCE_TIME CBlurayMenu::Tick(REFERENCE_TIME position, bool running, double rate, long volume)
{
    if (m->media.LossReason() || m->readFailed) { m->menuAudio.Reset(); return position; }
    const HRESULT audioResult = m->menuAudio.Tick(volume);
    if (FAILED(audioResult)) StreamResult(true, -1, audioResult);
    if (m->title && !m->waitingGraph) {
        position = m->clock.Update(position, GetTickCount64(), running, rate,
            m->SegmentEnd());
    }
    if (m->title && !m->waitingGraph && !m->completed && !m->clock.IsFallback() && m->lastPosition >= 0 &&
        (position < m->lastPosition - 5000000 || position > m->lastPosition + 20000000)) {
        m->QueuePlayerSeek(position);
    }
    m->lastPosition = position;
    if (m->playerSeekAfter) {
        // Keep Java graphics responsive but do not read from the old stream
        // position while another user seek is likely to replace this one.
        m->ConsumeArgb();
        m->Drain();
        if (m->playerSeekAfter && GetTickCount64() < m->playerSeekAfter) {
            m->Present();
            return position;
        }
        m->SyncPlayerSeek();
    }
    m->Pump(position);
    return position;
}
bool CBlurayMenu::HoldsMenuStill() const {
    return m->menuAudio.Active() && m->still && m->completed;
}
void CBlurayMenu::SetAudioState(OAFilterState state) {
    const HRESULT hr = m->menuAudio.SetState(state);
    if (FAILED(hr)) StreamResult(true, -1, hr);
}
OAFilterState CBlurayMenu::PlaybackState(OAFilterState mainState) const {
    return m->menuAudio.Active() ? m->menuAudio.PlaybackState() : mainState;
}
REFERENCE_TIME CBlurayMenu::PlaybackPosition(REFERENCE_TIME reported) const { return m->clock.Position(reported); }
void CBlurayMenu::PlayerSeek(REFERENCE_TIME position) {
    if (m->title && !m->waitingGraph) {
        const bool held = m->still && m->completed;
        m->QueuePlayerSeek(position);
        if (held) m->playbackRequest = 0; // Leave the navigator's automatic pause.
    }
}
bool CBlurayMenu::CanSkip() const {
    return m->bd && m->title && m->title->duration && !m->waitingGraph
        && !m->readFailed && !m->media.LossReason() && !MenuActive();
}
bool CBlurayMenu::Skip(bool forward, REFERENCE_TIME& position, unsigned& chapter, unsigned& chapterCount) {
    chapter = chapterCount = 0;
    if (!CanSkip()) return false;
    position = PlaybackPosition(position);
    // A next/previous-point operation has its own UO restriction. The graph
    // seek below is an implementation detail, not a user time-search request:
    // a disc can forbid time search while allowing a skip to its next chapter.
    const uint32_t mask = forward
        ? BLURAY_UO_SKIP_TO_NEXT_POINT_MASK : BLURAY_UO_SKIP_BACK_TO_PREVIOUS_POINT_MASK;
    if (m->Restricted(mask, BLURAY_PLAYER_SETTING_UO_RESTRICTION_COMPLIANT)) {
        m->notice = ResStr(IDS_BD_SKIP_FORBIDDEN);
        m->Log("skip_unavailable", forward, m->uoMask);
        return false;
    }
    REFERENCE_TIME target = forward ? REFERENCE_TIME(m->title->duration * 1000 / 9) : 0;
    const REFERENCE_TIME threshold = forward ? position + 10000 : std::max<REFERENCE_TIME>(0, position - 30000000);
    for (unsigned i = 0; i < m->title->chapter_count; ++i) {
        const auto start = REFERENCE_TIME(m->title->chapters[i].start * 1000 / 9);
        if (forward && start > threshold) { target = start; chapter = i + 1; break; }
        if (!forward && start <= threshold) { target = start; chapter = i + 1; }
    }
    // At the last chapter, finish this playlist. libbluray then follows the
    // authored end action (next intro, menu, etc.), rather than opening a file
    // selected by MPC-BE's ordinary folder/playlist navigation.
    m->Log(forward ? "skip_next" : "skip_previous", position, target);
    chapterCount = m->title->chapter_count;
    m->Log("skip_chapter", chapter, chapterCount);
    position = target;
    return true;
}
void CBlurayMenu::Complete() { if (m->title) m->completed = true; m->Log("graph_complete", m->playlist, m->lastPosition); }
bool CBlurayMenu::GraphComplete() {
    if (m->title && !m->clock.AcceptEnd(m->SegmentEnd())) {
        m->Log("graph_complete_early", m->playlist, m->lastPosition);
        return false;
    }
    Complete();
    return true;
}
bool CBlurayMenu::TakePlaybackRequest(bool& paused) {
    if (m->playbackRequest < 0 || m->waitingGraph) return false;
    paused = m->playbackRequest != 0; m->playbackRequest = -1; return true;
}
bool CBlurayMenu::NeedsPlaybackPause(REFERENCE_TIME position) const {
    if (m->completed || !m->title) return false;
    if (m->still && m->playitem < m->title->clip_count) {
        const auto& clip = m->title->clips[m->playitem];
        const auto end = REFERENCE_TIME((clip.start_time + clip.out_time - clip.in_time) * 1000 / 9);
        return position >= end - 10000; // Allow integer conversion rounding (1 ms).
    }
    // Some renderers delay EC_COMPLETE after the authored end. Navigation has
    // reached EOS and the presentation clock has played the entire playlist.
    return m->readEnd && position >= REFERENCE_TIME(m->title->duration * 1000 / 9) - 10000;
}
bool CBlurayMenu::PlaybackRange(REFERENCE_TIME position, REFERENCE_TIME& stop, UINT& endPlayItem) {
    if (!m->title) return false;
    stop = REFERENCE_TIME(m->title->duration * 1000 / 9);
    endPlayItem = m->title->clip_count;
    for (unsigned i = 0; i < m->title->clip_count; ++i) {
        const auto& clip = m->title->clips[i];
        const auto end = REFERENCE_TIME((clip.start_time + clip.out_time - clip.in_time) * 1000 / 9);
        if (clip.still_mode && end > position + 10000) {
            stop = end;
            endPlayItem = i + 1;
            break;
        }
    }
    m->segmentStop = stop;
    m->Log("playback_range", position, stop);
    return true;
}
void CBlurayMenu::PlaybackFailed(HRESULT result) {
    m->Log("playback_failed", result);
    m->error.Format(ResStr(IDS_BD_PLAYLIST_SEGMENT_FAILED), unsigned(result));
}
bool CBlurayMenu::MenuVisible() const { return m->MenuVisible(); }
bool CBlurayMenu::MenuActive() const { return m->MenuActive(); }
bool CBlurayMenu::CanShowMenu(bool popup) const {
    return !m->firstPlayPending && !m->waitingGraph && !m->readFailed && !m->media.LossReason() && (popup
        ? (m->popupAvailable || m->bdjActive) && !m->Restricted(MenuVisible() ? BLURAY_UO_POPUP_OFF_MASK : BLURAY_UO_POPUP_ON_MASK, BLURAY_PLAYER_SETTING_UO_RESTRICTION_COMPLIANT)
        : m->topMenuAvailable && !m->Restricted(BLURAY_UO_MENU_CALL, BLURAY_PLAYER_SETTING_UO_RESTRICTION_SAFE));
}
CStringW CBlurayMenu::TakeNotice() {
    CStringW notice = m->notice; m->notice.Empty(); return notice;
}
bool CBlurayMenu::TakeStreams(int& audioPid, int& subtitlePid) {
    audioPid = subtitlePid = -2;
    if (!m->streamsDirty || m->waitingGraph || !m->title || m->playitem >= m->title->clip_count) return false;
    m->streamsDirty = false;
    const auto& clip = m->title->clips[m->playitem];
    BlurayMenuAudio separateAudio;
    if (GetBlurayMenuAudio(m->audioPlaylist, m->playitem, m->audioStream, separateAudio)) {
        const HRESULT hr = m->menuAudio.Select(m->root, separateAudio);
        if (hr != S_FALSE) {
            m->Log("menu_audio_subpath", separateAudio.subpath, separateAudio.pid);
            StreamResult(true, separateAudio.pid, SUCCEEDED(hr) ? S_OK : hr);
        }
        m->lastAudioPid = -2; // The same PID in the main mux is a different stream.
    } else {
        m->menuAudio.Reset();
        if (m->audioStream && m->audioStream <= clip.audio_stream_count) {
            const int pid = clip.audio_streams[m->audioStream - 1].pid;
            if (pid != m->lastAudioPid) audioPid = m->lastAudioPid = pid;
        }
    }
    int pid = -1;
    if (m->pgEnabled && m->pgStream && m->pgStream <= clip.pg_stream_count)
        pid = clip.pg_streams[m->pgStream - 1].pid;
    if (pid != m->lastSubtitlePid) subtitlePid = m->lastSubtitlePid = pid;
    return audioPid != -2 || subtitlePid != -2;
}
void CBlurayMenu::StreamResult(bool audio, int pid, HRESULT result) {
    m->Log(audio ? "audio_selected" : "subtitle_selected", pid, result);
    if (result != S_OK) m->notice = audio
        ? ResStr(IDS_BD_AUDIO_SELECTION_FAILED)
        : ResStr(IDS_BD_SUBTITLE_SELECTION_FAILED);
}
void CBlurayMenu::PlaylistReused() { m->Log("playlist_reused", m->playlist); }
bool CBlurayMenu::TakePlaylist(CStringW& path) {
    if (m->readFailed || m->media.LossReason() || m->pendingPlaylist.IsEmpty()) return false;
    path = m->pendingPlaylist; m->pendingPlaylist.Empty(); return true;
}
bool CBlurayMenu::TakeSeek(REFERENCE_TIME& position) {
    if (m->pendingSeek < 0) return false;
    position = m->pendingSeek; m->pendingSeek = -1; return true;
}
void CBlurayMenu::SeekApplied(REFERENCE_TIME position) {
    m->playerSeekAfter = 0;
    m->lastPosition = position;
    m->clock.Seek(position, GetTickCount64());
}
CStringW CBlurayMenu::DiscName() const { return m->discName; }
CStringW CBlurayMenu::StorageKey() const { return m->media.LossReason() ? CStringW() : m->storageKey; }
CStringW CBlurayMenu::StoragePath() const { return m->storagePath; }
CStringW CBlurayMenu::DataDirectory() {
    WCHAR path[32768]{}; GetModuleFileNameW(nullptr, path, _countof(path));
    CStringW directory(path); directory.Truncate(directory.ReverseFind(L'\\') + 1);
    return directory + L"bdj-data";
}
CStringW CBlurayMenu::Error() const { return m->error; }
void CBlurayMenu::DeviceChange(UINT event, DWORD drives) { m->media.DeviceChange(event, drives); }
CStringW CBlurayMenu::DiscLossNotice() const {
    const auto reason = m->media.LossReason();
    if (!reason && !m->readFailed) return CStringW();
    m->Log("disc_unavailable", reason, m->media.Error());
    return reason ? ResStr(IDS_BD_DISC_CHANGED)
        : ResStr(IDS_BD_DISC_READ_ERROR);
}
bool CBlurayMenu::Key(UINT key)
{
    if (!m->bd || m->firstPlayPending || m->waitingGraph || m->readFailed || m->media.LossReason()) return false;
    uint32_t code;
    if (key == VK_HOME || key == VK_APPS) {
        const bool popup = key == VK_APPS;
        if (!CanShowMenu(popup)) {
            m->notice = popup ? ResStr(IDS_BD_POPUP_UNAVAILABLE)
                : ResStr(IDS_BD_TOP_MENU_FORBIDDEN);
            m->Log("menu_unavailable", key, m->uoMask);
            return true;
        }
        code = popup ? BD_VK_POPUP : BD_VK_ROOT_MENU;
    }
    else if (key == VK_ESCAPE && MenuActive() && (m->popupAvailable || m->bdjActive)) code = BD_VK_POPUP;
    else if (MenuActive()) {
        switch (key) {
        case VK_UP: code = BD_VK_UP; break;
        case VK_DOWN: code = BD_VK_DOWN; break;
        case VK_LEFT: code = BD_VK_LEFT; break;
        case VK_RIGHT: code = BD_VK_RIGHT; break;
        case VK_RETURN: code = BD_VK_ENTER; break;
        default: return false;
        }
    } else return false;
    if (m->presenting) return true;
    // A menu request must see the latest playback position immediately.
    m->SyncPlayerSeek();
    const int result = m->bd_user_input(m->bd, m->pts, code);
    m->Log("key", key, result);
    m->RunInputCommands();
    m->Present();
    return true;
}
bool CBlurayMenu::Mouse(CPoint point, const CRect& rendererRect, bool activate)
{
    if (!MenuActive() || m->waitingGraph || !m->osd || m->readFailed || m->media.LossReason()) return false;
    if (m->presenting) return true;
    m->SyncPlayerSeek();
    const auto& plane = m->planes[BD_OVERLAY_IG];
    CRect output, active;
    if (FAILED(m->osd->OsdGetVideoRects(&output, &active))) return false;
    if (rendererRect != m->lastMouseRendererRect || active != m->lastMouseVideoRect) {
        m->lastMouseRendererRect = rendererRect; m->lastMouseVideoRect = active;
        m->Log("mouse_renderer_origin", rendererRect.left, rendererRect.top);
        m->Log("mouse_renderer_size", rendererRect.Width(), rendererRect.Height());
        m->Log("mouse_video_origin", active.left, active.top);
        m->Log("mouse_video_size", active.Width(), active.Height());
    }
    POINT mapped{};
    if (activate) m->Log("mouse_down", point.x, point.y);
    if (!MapBlurayMenuPoint(point, rendererRect, active, plane.width, plane.height, mapped)) {
        if (activate) m->Log("mouse_outside");
        return false;
    }
    // With no visible Java graphics there is no mouse target to hit. Treat
    // one click on the looping background as a wake key, without clicking
    // through to whichever menu button the Java application restores.
    if (m->bdjActive && m->menuBackground && !MenuVisible() && activate) {
        m->Log("background_wake_click", mapped.x, mapped.y);
        return Key(VK_RETURN);
    }
    int selected = m->bd_mouse_select(m->bd, m->pts, uint16_t(mapped.x), uint16_t(mapped.y));
    if (m->bdjActive) {
        if (activate) {
            const int result = m->bd_user_input(m->bd, m->pts, BD_VK_MOUSE_ACTIVATE);
            m->Log("bdj_mouse_click", result);
            m->RunInputCommands();
        }
        m->Present();
        return true;
    }
    if (activate && selected == 0) {
        // Some submenus expose their return only as an invisible arrow-key
        // target. The local libbluray extension checks the disc's page links
        // and the destination button geometry before queuing that return.
        const int returned = m->bd_mouse_select_page(m->bd, m->pts, uint16_t(mapped.x), uint16_t(mapped.y));
        m->Log("mouse_page_return", returned);
        if (returned > 0 && m->RunInputCommands() && m->menu && !m->waitingGraph) {
            selected = m->bd_mouse_select(m->bd, m->pts, uint16_t(mapped.x), uint16_t(mapped.y));
        }
    }
    if (activate) {
        m->Log("mouse_disc", mapped.x, mapped.y);
        m->Log("mouse_hit", selected);
    }
    if (activate && selected > 0) Key(VK_RETURN);
    m->Present();
    return true;
}
