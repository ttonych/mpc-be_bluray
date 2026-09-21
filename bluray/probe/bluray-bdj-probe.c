/* Bounded BD-J startup / ARGB probe. No desktop input or video decoding. */
#define main hdmv_probe_main
#include "bluray-menu-probe.c"
#undef main

typedef struct {
    Probe p;
    CRITICAL_SECTION lock;
    ULONGLONG last_snapshot;
    unsigned argb_frames;
} JavaProbe;

static void argb_overlay(void *context, const BD_ARGB_OVERLAY *ov)
{
    JavaProbe *s = context;
    EnterCriticalSection(&s->lock);
    if (!ov) {
        for (unsigned i = 0; i < 3; ++i) release_plane(&s->p.planes[i]);
    } else if (ov->plane < 3) {
        Plane *p = &s->p.planes[ov->plane];
        if (ov->cmd == BD_ARGB_OVERLAY_CLOSE) release_plane(p);
        else if (ov->cmd == BD_ARGB_OVERLAY_INIT && ov->w && ov->h && ov->w <= 4096 && ov->h <= 2160) {
            release_plane(p); p->w = ov->w; p->h = ov->h;
            p->bgra = calloc((size_t)p->w * p->h, 4);
            printf("{\"type\":\"argb_init\",\"plane\":%u,\"w\":%u,\"h\":%u}\n", ov->plane, p->w, p->h);
        } else if (ov->cmd == BD_ARGB_OVERLAY_DRAW && p->bgra && ov->argb &&
                   (unsigned)ov->x + ov->w <= p->w && (unsigned)ov->y + ov->h <= p->h && ov->stride >= ov->w) {
            for (unsigned y = 0; y < ov->h; ++y)
                memcpy(p->bgra + ((size_t)(y + ov->y) * p->w + ov->x) * 4,
                       ov->argb + (size_t)y * ov->stride, (size_t)ov->w * 4);
        } else if (ov->cmd == BD_ARGB_OVERLAY_FLUSH && p->bgra) {
            s->argb_frames++;
            if (GetTickCount64() - s->last_snapshot >= 1000) {
                snapshot(&s->p, p, ov->plane, ov->pts);
                s->last_snapshot = GetTickCount64();
            }
        }
    }
    LeaveCriticalSection(&s->lock);
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "Usage: %s DISC OUTPUT JAVA_HOME [SECONDS] [COUNTRY] [REGION]\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IOLBF, 4096);
    JavaProbe s = {0};
    InitializeCriticalSection(&s.lock);
    s.p.output = argv[2]; s.p.phase = "bdj";
    BLURAY *bd = bd_init();
    if (!bd) return 2;
    bd_set_player_setting_str(bd, BLURAY_PLAYER_JAVA_HOME, argv[3]);
    bd_set_player_setting_str(bd, BLURAY_PLAYER_SETTING_MENU_LANG, "eng");
    bd_set_player_setting_str(bd, BLURAY_PLAYER_SETTING_AUDIO_LANG, "eng");
    if (argc > 5) bd_set_player_setting_str(bd, BLURAY_PLAYER_SETTING_COUNTRY_CODE, argv[5]);
    if (argc > 6) bd_set_player_setting(bd, BLURAY_PLAYER_SETTING_REGION_CODE, strtoul(argv[6], NULL, 10));
    bd_set_player_setting(bd, BLURAY_PLAYER_SETTING_PLAYER_PROFILE, BLURAY_PLAYER_PROFILE_1_v1_0);
    bd_set_player_setting(bd, BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE, 0);
    if (!bd_open_disc(bd, argv[1], NULL)) { bd_close(bd); return 2; }
    const BLURAY_DISC_INFO *info = bd_get_disc_info(bd);
    printf("{\"type\":\"disc\",\"bdj\":%u,\"handled\":%u,\"first_play\":%u,\"top_menu\":%u}\n",
           info->num_bdj_titles, info->bdj_handled, info->first_play_supported, info->top_menu_supported);
    bd_get_event(bd, NULL);
    bd_register_argb_overlay_proc(bd, &s, argb_overlay, NULL);
    if (!bd_play(bd)) { fprintf(stderr, "BD-J first play failed\n"); bd_close(bd); return 3; }
    ULONGLONG end = GetTickCount64() + (argc > 4 ? strtoul(argv[4], NULL, 10) : 40) * 1000;
    ULONGLONG started = GetTickCount64();
    BLURAY_TITLE_INFO *title = NULL;
    uint8_t buf[6144];
    while (GetTickCount64() < end && !s.p.errors) {
        uint64_t position = (GetTickCount64() - started) * 90;
        if (title && position > title->duration) position = title->duration;
        if (title && title->clip_count) {
            unsigned item = 0;
            while (item + 1 < title->clip_count && title->clips[item + 1].start_time <= position) ++item;
            bd_set_scr(bd, (int64_t)title->clips[item].in_time + position - title->clips[item].start_time);
        }
        BD_EVENT e = {0};
        int n = bd_read_ext(bd, buf, sizeof(buf), &e);
        if (n < 0) { s.p.errors++; break; }
        s.p.total_bytes += n;
        do {
            if (e.event == BD_EVENT_PLAYLIST) {
                if (title) bd_free_title_info(title);
                title = bd_get_playlist_info(bd, e.param, 0);
                started = GetTickCount64();
            }
            if (e.event != BD_EVENT_IDLE) handle_event(&s.p, bd, e);
        } while (bd_get_event(bd, &e));
        if (!n || (title && bd_tell_time(bd) > position + 9000)) Sleep(20);
    }
    bd_close(bd);
    if (title) bd_free_title_info(title);
    printf("{\"type\":\"summary\",\"argb_frames\":%u,\"errors\":%u,\"bytes\":%" PRIu64 "}\n",
           s.argb_frames, s.p.errors, s.p.total_bytes);
    for (unsigned i = 0; i < 3; ++i) release_plane(&s.p.planes[i]);
    DeleteCriticalSection(&s.lock);
    return s.p.errors || !s.argb_frames ? 1 : 0;
}
