/* Scripted component test of libbluray mouse navigation (no desktop input).
 * Reuse the bounded reader and overlay recorder from the first-play probe.
 * SPDX-License-Identifier: MIT
 */
#define main first_play_probe_main
#include "bluray-menu-probe.c"
#undef main

static void flush_commands(Probe *s, BLURAY *bd)
{
    for (unsigned i = 0; i < 128 && !s->errors; ++i) {
        BD_EVENT e = {0};
        if (bd_read_ext(bd, NULL, 0, &e) < 0) { s->errors++; break; }
        handle_event(s, bd, e);
        if (!e.event) break;
    }
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s DISC OUTPUT_DIRECTORY SCRIPT\n", argv[0]);
        return 2;
    }
    FILE *script = fopen(argv[3], "r");
    if (!script) return 2;
    Probe s = {0};
    s.output = argv[2]; s.phase = "first-play";
    s.deadline = GetTickCount64() + 20000;
    BLURAY *bd = bd_open(argv[1], NULL);
    if (!bd) { fclose(script); return 2; }
    bd_get_event(bd, NULL);
    bd_register_overlay_proc(bd, &s, overlay);
    if (!bd_play(bd)) s.errors++;
    pump(&s, bd, 16000, 1);
    if (!s.ig_frames) s.errors++;
    char line[256], command[32], phase[64];
    unsigned x, y, activate, flush, key;
    while (!s.errors && fgets(line, sizeof(line), script)) {
        if (line[0] == '#' || sscanf(line, "%31s %63s", command, phase) != 2) continue;
        s.phase = phase;
        fprintf(stderr, "PROBE %s\n", line);
        if ((!strcmp(command, "mouse") || !strcmp(command, "pageclick")) &&
            sscanf(line, "%*s %*s %u %u %u %u", &x, &y, &activate, &flush) == 4) {
            int hit = bd_mouse_select(bd, -1, (uint16_t)x, (uint16_t)y);
            printf("{\"type\":\"mouse\",\"phase\":\"%s\",\"x\":%u,\"y\":%u,\"hit\":%d}\n", phase, x, y, hit);
            if (activate && hit == 0 && !strcmp(command, "pageclick")) {
                int changed = bd_mouse_select_page(bd, -1, (uint16_t)x, (uint16_t)y);
                printf("{\"type\":\"page_return\",\"phase\":\"%s\",\"result\":%d}\n", phase, changed);
                if (changed > 0) {
                    flush_commands(&s, bd);
                    hit = bd_mouse_select(bd, -1, (uint16_t)x, (uint16_t)y);
                    printf("{\"type\":\"mouse_after_return\",\"phase\":\"%s\",\"hit\":%d}\n", phase, hit);
                }
            }
            if (flush) flush_commands(&s, bd);
            if (activate && hit > 0) {
                bd_user_input(bd, -1, BD_VK_ENTER);
                flush_commands(&s, bd);
            }
        } else if (!strcmp(command, "key") && sscanf(line, "%*s %*s %u", &key) == 1) {
            int result = bd_user_input(bd, -1, key);
            printf("{\"type\":\"key\",\"phase\":\"%s\",\"key\":%u,\"result\":%d}\n", phase, key, result);
            flush_commands(&s, bd);
        } else if (!strcmp(command, "flush")) {
            flush_commands(&s, bd);
        } else { fprintf(stderr, "Invalid probe command: %s", line); s.errors++; }
    }
    printf("{\"type\":\"summary\",\"errors\":%u,\"playlist\":%u,\"ig_frames\":%u}\n", s.errors, s.playlist, s.ig_frames);
    fclose(script);
    bd_close(bd);
    for (unsigned i = 0; i < 3; ++i) release_plane(&s.planes[i]);
    return s.errors ? 1 : 0;
}
