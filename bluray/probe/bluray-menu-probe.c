/* A bounded HDMV integration probe; does not decode or render video.
 * SPDX-License-Identifier: MIT
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <libbluray/bluray.h>
#include <libbluray/keys.h>
#include <libbluray/overlay.h>
#include <libbluray/player_settings.h>

typedef struct {
    unsigned w, h;
    uint8_t *indices, *bgra;
    BD_PG_PALETTE_ENTRY palette[256];
    uint64_t hash;
} Plane;

typedef struct {
    Plane planes[3];
    const char *output;
    unsigned overlay_calls, ig_frames, snapshots, errors;
    unsigned menu, playlist, playlist_changes;
    uint64_t total_bytes;
    ULONGLONG deadline;
    const char *phase;
} Probe;

static uint8_t clamp_color(double x)
{
    return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x + 0.5);
}

static void paint_pixel(Plane *p, size_t at, unsigned color)
{
    const BD_PG_PALETTE_ENTRY *c = &p->palette[color];
    uint8_t *out = &p->bgra[at * 4];
    double y = 1.164383 * ((int)c->Y - 16);
    double cb = (int)c->Cb - 128, cr = (int)c->Cr - 128;
    p->indices[at] = (uint8_t)color;
    /* Diagnostic preview assumes limited-range BT.709 for this HD disc.
     * A player must select the matrix from the associated video stream. */
    out[0] = clamp_color(y + 2.112402 * cb);
    out[1] = clamp_color(y - 0.213249 * cb - 0.532909 * cr);
    out[2] = clamp_color(y + 1.792741 * cr);
    out[3] = color == 255 ? 0 : c->T;
}

static void release_plane(Plane *p)
{
    free(p->indices);
    free(p->bgra);
    memset(p, 0, sizeof(*p));
}

static void snapshot(Probe *s, Plane *p, unsigned plane, int64_t pts)
{
    size_t n = (size_t)p->w * p->h, visible = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < n; i++) {
        if (p->bgra[i * 4 + 3]) visible++;
        for (unsigned k = 0; k < 4; k++) {
            /* Transparent RGB does not affect the displayed image. */
            uint8_t v = p->bgra[i * 4 + 3] ? p->bgra[i * 4 + k] : 0;
            hash = (hash ^ v) * UINT64_C(1099511628211);
        }
    }
    int changed = p->hash != hash;
    p->hash = hash;
    if (plane == BD_OVERLAY_IG && visible) s->ig_frames++;
    printf("{\"type\":\"overlay_flush\",\"phase\":\"%s\",\"plane\":%u,"
           "\"width\":%u,\"height\":%u,\"pts\":%" PRId64 ",\"visible_pixels\":%zu,"
           "\"changed\":%d,\"hash\":\"%016" PRIx64 "\"}\n",
           s->phase, plane, p->w, p->h, pts, visible, changed, hash);
    if (!changed || !visible || s->snapshots >= 16) return;
    char path[1024];
    int path_len = snprintf(path, sizeof(path), "%s/menu-%02u-%s-plane%u.tga",
                           s->output, s->snapshots, s->phase, plane);
    if (path_len < 0 || path_len >= (int)sizeof(path)) { s->errors++; return; }
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); s->errors++; return; }
    uint8_t header[18] = {0};
    header[2] = 2; /* uncompressed true color */
    header[12] = (uint8_t)p->w; header[13] = (uint8_t)(p->w >> 8);
    header[14] = (uint8_t)p->h; header[15] = (uint8_t)(p->h >> 8);
    header[16] = 32; header[17] = 0x28; /* top left, 8 alpha bits */
    int ok = fwrite(header, 1, sizeof(header), f) == sizeof(header);
    ok = (fwrite(p->bgra, 4, n, f) == n) && ok;
    if (fclose(f) || !ok) s->errors++;
    else s->snapshots++;
}

static void overlay(void *context, const BD_OVERLAY *ov)
{
    Probe *s = context;
    if (!ov) {
        for (unsigned i = 0; i < 3; i++) release_plane(&s->planes[i]);
        return;
    }
    s->overlay_calls++;
    if (ov->plane >= 3) { s->errors++; return; }
    Plane *p = &s->planes[ov->plane];
    if (ov->cmd == BD_OVERLAY_CLOSE) { release_plane(p); return; }
    if (ov->cmd == BD_OVERLAY_INIT) {
        release_plane(p);
        if (!ov->w || !ov->h || ov->w > 4096 || ov->h > 2160) { s->errors++; return; }
        p->w = ov->w; p->h = ov->h;
        size_t n = (size_t)p->w * p->h;
        p->indices = malloc(n);
        p->bgra = calloc(n, 4);
        if (!p->indices || !p->bgra) { release_plane(p); s->errors++; return; }
        memset(p->indices, 255, n);
        printf("{\"type\":\"overlay_init\",\"plane\":%u,\"width\":%u,\"height\":%u}\n",
               ov->plane, p->w, p->h);
        return;
    }
    if (!p->bgra) { s->errors++; return; }
    if (ov->cmd == BD_OVERLAY_FLUSH) { snapshot(s, p, ov->plane, ov->pts); return; }
    if (ov->cmd == BD_OVERLAY_CLEAR || ov->cmd == BD_OVERLAY_HIDE) {
        memset(p->indices, 255, (size_t)p->w * p->h);
        memset(p->bgra, 0, (size_t)p->w * p->h * 4);
        return;
    }
    if ((unsigned)ov->x + ov->w > p->w || (unsigned)ov->y + ov->h > p->h) {
        s->errors++; return;
    }
    if (ov->cmd == BD_OVERLAY_WIPE) {
        for (unsigned y = ov->y; y < (unsigned)ov->y + ov->h; y++) {
            size_t at = (size_t)y * p->w + ov->x;
            memset(p->indices + at, 255, ov->w);
            memset(p->bgra + at * 4, 0, (size_t)ov->w * 4);
        }
        return;
    }
    if (ov->cmd != BD_OVERLAY_DRAW) { s->errors++; return; }
    if (ov->palette) memcpy(p->palette, ov->palette, sizeof(p->palette));
    if (!ov->img) {
        for (unsigned y = 0; y < ov->h; y++)
            for (unsigned x = 0; x < ov->w; x++) {
                size_t at = (size_t)(ov->y + y) * p->w + ov->x + x;
                paint_pixel(p, at, p->indices[at]);
            }
        return;
    }
    const BD_PG_RLE_ELEM *rle = ov->img;
    size_t budget = (size_t)ov->w * ov->h * 2 + ov->h;
    for (unsigned y = 0; y < ov->h; y++) {
        unsigned x = 0;
        while (x < ov->w) {
            if (!budget--) { s->errors++; return; }
            BD_PG_RLE_ELEM run = *rle++;
            if (!run.len) continue;
            if (run.color > 255 || run.len > ov->w - x) {
                s->errors++; return;
            }
            for (unsigned j = 0; j < run.len; j++)
                paint_pixel(p, (size_t)(ov->y + y) * p->w + ov->x + x + j, run.color);
            x += run.len;
        }
        /* Empty crop runs and EOL markers are skipped before the next pixel.
         * Never peek beyond the final pixel for an optional trailing EOL. */
    }
}

static const char *event_name(unsigned event)
{
    switch (event) {
    case BD_EVENT_ERROR: return "ERROR";
    case BD_EVENT_READ_ERROR: return "READ_ERROR";
    case BD_EVENT_ENCRYPTED: return "ENCRYPTED";
    case BD_EVENT_TITLE: return "TITLE";
    case BD_EVENT_PLAYLIST: return "PLAYLIST";
    case BD_EVENT_PLAYITEM: return "PLAYITEM";
    case BD_EVENT_STILL: return "STILL";
    case BD_EVENT_STILL_TIME: return "STILL_TIME";
    case BD_EVENT_MENU: return "MENU";
    case BD_EVENT_POPUP: return "POPUP";
    case BD_EVENT_SOUND_EFFECT: return "SOUND_EFFECT";
    case BD_EVENT_END_OF_TITLE: return "END_OF_TITLE";
    case BD_EVENT_PLAYLIST_STOP: return "PLAYLIST_STOP";
    default: return "OTHER";
    }
}

static void handle_event(Probe *s, BLURAY *bd, BD_EVENT e)
{
    if (!e.event) return;
    printf("{\"type\":\"event\",\"phase\":\"%s\",\"name\":\"%s\",\"event\":%u,\"param\":%u}\n",
           s->phase, event_name(e.event), e.event, e.param);
    if (e.event == BD_EVENT_ERROR || e.event == BD_EVENT_ENCRYPTED || e.event == BD_EVENT_READ_ERROR) s->errors++;
    if (e.event == BD_EVENT_MENU) s->menu = e.param;
    if (e.event == BD_EVENT_PLAYLIST) {
        s->playlist = e.param;
        s->playlist_changes++;
        BLURAY_TITLE_INFO *info = bd_get_playlist_info(bd, e.param, 0);
        if (info) {
            printf("{\"type\":\"playlist_info\",\"playlist\":%u,\"duration_90khz\":%" PRIu64
                   ",\"clips\":%u,\"first_clip\":\"%.5s\"}\n", e.param, info->duration,
                   info->clip_count, info->clip_count ? info->clips[0].clip_id : "");
            bd_free_title_info(info);
        }
    }
    /* This probe has no A/V clock; timed stills are skipped deliberately.
     * Infinite stills are retained for menu interaction. */
    if (e.event == BD_EVENT_STILL_TIME && e.param) bd_read_skip_still(bd);
}

static void pump(Probe *s, BLURAY *bd, unsigned max_reads, int stop_on_menu)
{
    uint8_t buf[6144];
    for (unsigned i = 0; i < max_reads && !s->errors; i++) {
        if (GetTickCount64() >= s->deadline || s->total_bytes >= UINT64_C(128) * 1024 * 1024) break;
        BD_EVENT e = {0};
        int n = bd_read_ext(bd, buf, sizeof(buf), &e);
        if (n < 0) { s->errors++; break; }
        s->total_bytes += (unsigned)n;
        handle_event(s, bd, e);
        while (bd_get_event(bd, &e)) handle_event(s, bd, e);
        if (stop_on_menu && s->ig_frames && s->menu) break;
        if (!n && !e.event) Sleep(1);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "Usage: %s DISC_ROOT OUTPUT_DIRECTORY\n", argv[0]); return 2; }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, NULL, _IOLBF, 4096);
    Probe s = {0};
    s.output = argv[2]; s.phase = "first-play"; s.deadline = GetTickCount64() + 20000;
    int major, minor, micro;
    bd_get_version(&major, &minor, &micro);
    printf("{\"type\":\"version\",\"version\":\"%d.%d.%d\"}\n", major, minor, micro);
    if (major != 1 || minor != 5 || micro != 0) { fprintf(stderr, "Expected libbluray 1.5.0\n"); return 3; }
    BLURAY *bd = bd_open(argv[1], NULL);
    if (!bd) { fprintf(stderr, "Could not open disc\n"); return 4; }
    const BLURAY_DISC_INFO *disc = bd_get_disc_info(bd);
    if (!disc || !disc->bluray_detected) { bd_close(bd); return 4; }
    printf("{\"type\":\"disc\",\"hdmv_titles\":%u,\"bdj_titles\":%u,\"first_play\":%u,"
           "\"top_menu\":%u,\"aacs\":%u,\"bdplus\":%u}\n", disc->num_hdmv_titles,
           disc->num_bdj_titles, disc->first_play_supported, disc->top_menu_supported,
           disc->aacs_detected, disc->bdplus_detected);
    if (disc->num_bdj_titles) { fprintf(stderr, "This probe validates HDMV only\n"); bd_close(bd); return 5; }
    bd_set_player_setting_str(bd, BLURAY_PLAYER_SETTING_MENU_LANG, "eng");
    bd_set_player_setting_str(bd, BLURAY_PLAYER_SETTING_AUDIO_LANG, "eng");
    bd_set_player_setting_str(bd, BLURAY_PLAYER_SETTING_PG_LANG, "eng");
    bd_get_event(bd, NULL);
    bd_register_overlay_proc(bd, &s, overlay);
    int started = bd_play(bd);
    printf("{\"type\":\"play\",\"result\":%d}\n", started);
    if (!started) s.errors++;
    pump(&s, bd, 16000, 1);
    if (!s.ig_frames && !s.errors) {
        s.phase = "top-menu";
        int result = bd_menu_call(bd, -1);
        printf("{\"type\":\"menu_call\",\"result\":%d}\n", result);
        pump(&s, bd, 4000, 1);
    }
    uint64_t initial_hash = s.planes[BD_OVERLAY_IG].hash;
    unsigned menu_playlist = s.playlist, initial_frames = s.ig_frames;
    int key_result = 0, changed = 0, enter_result = 0;
    if (s.ig_frames && !s.errors) {
        unsigned keys[] = {BD_VK_DOWN, BD_VK_RIGHT, BD_VK_UP, BD_VK_LEFT};
        const char *names[] = {"down", "right", "up", "left"};
        for (unsigned i = 0; i < 4; i++) {
            s.phase = names[i];
            key_result = bd_user_input(bd, -1, keys[i]);
            printf("{\"type\":\"input\",\"key\":\"%s\",\"result\":%d}\n", names[i], key_result);
            pump(&s, bd, 4, 0);
            if (s.planes[BD_OVERLAY_IG].hash != initial_hash) changed = 1;
        }
        s.phase = "enter";
        enter_result = bd_user_input(bd, -1, BD_VK_ENTER);
        printf("{\"type\":\"input\",\"key\":\"enter\",\"result\":%d}\n", enter_result);
        pump(&s, bd, 200, 0);
    }
    int passed = started && initial_frames && changed && enter_result > 0 && !s.errors;
    printf("{\"type\":\"summary\",\"passed\":%s,\"overlay_calls\":%u,\"ig_frames\":%u,"
           "\"snapshots\":%u,\"direction_changed_graphics\":%s,\"enter_result\":%d,"
           "\"menu_playlist\":%u,\"final_playlist\":%u,\"bytes_read\":%" PRIu64 ",\"errors\":%u}\n",
           passed ? "true" : "false", s.overlay_calls, s.ig_frames, s.snapshots,
           changed ? "true" : "false", enter_result, menu_playlist, s.playlist, s.total_bytes, s.errors);
    bd_close(bd);
    for (unsigned i = 0; i < 3; i++) release_plane(&s.planes[i]);
    return passed ? 0 : 1;
}
