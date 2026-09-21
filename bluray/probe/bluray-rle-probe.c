/* Capture libbluray overlay runs without a desktop renderer. SPDX-License-Identifier: MIT */
#define main original_hdmv_main
#include "bluray-menu-probe.c"
#undef main

static unsigned draws;
static void inspect_overlay(void *context, const BD_OVERLAY *ov)
{
    Probe *s = context;
    if (ov && ov->cmd == BD_OVERLAY_DRAW && ov->img && ov->w && ov->h) {
        unsigned pixels = (unsigned)ov->w * ov->h, total = 0, count = 0;
        unsigned row = 0, x = 0, crossing = 0, early_eol = 0;
        char name[1024];
        snprintf(name, sizeof(name), "%s/rle-%03u.bin", s->output, draws++);
        FILE *f = fopen(name, "wb");
        if (!f) {s->errors++;return;}
        uint32_t header[4] = {ov->x, ov->y, ov->w, ov->h};
        fwrite(header, sizeof(header), 1, f);
        while (total < pixels && count < pixels * 2 + ov->h) {
            BD_PG_RLE_ELEM run = ov->img[count++];
            fwrite(&run, sizeof(run), 1, f);
            if (!run.len) {
                if (x && x != ov->w) early_eol++;
                if (x == ov->w) { x = 0; row++; }
                continue;
            }
            if (x == ov->w) { x = 0; row++; }
            if (x + run.len > ov->w && !crossing++)
                printf("{\"type\":\"crossing\",\"row\":%u,\"x\":%u,\"len\":%u,\"color\":%u,\"width\":%u}\n",row,x,run.len,run.color,ov->w);
            x += run.len; total += run.len;
        }
        fclose(f);
        printf("{\"type\":\"rle\",\"draw\":%u,\"width\":%u,\"height\":%u,\"runs\":%u,\"pixels\":%u,\"crossing\":%u,\"early_eol\":%u}\n",draws-1,ov->w,ov->h,count,total,crossing,early_eol);
    }
    overlay(context, ov);
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    setvbuf(stdout, NULL, _IOLBF, 4096);
    Probe s = {0}; s.output=argv[2];s.phase="rle";s.deadline=GetTickCount64()+30000;
    BLURAY *bd=bd_init();if(!bd) return 2;
    bd_set_player_setting(bd,BLURAY_PLAYER_SETTING_REGION_CODE,1);
    bd_set_player_setting(bd,BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE,0);
    bd_set_player_setting_str(bd,BLURAY_PLAYER_SETTING_MENU_LANG,"eng");
    bd_set_player_setting_str(bd,BLURAY_PLAYER_SETTING_COUNTRY_CODE,"US");
    if(!bd_open_disc(bd,argv[1],NULL)){bd_close(bd);return 3;}
    bd_get_event(bd,NULL);bd_register_overlay_proc(bd,&s,inspect_overlay);
    int played=bd_play(bd);
    ULONGLONG started=GetTickCount64(), end=started+45000;
    BLURAY_TITLE_INFO *title=NULL;
    uint8_t buf[6144];
    while(GetTickCount64()<end && !s.errors && !s.ig_frames) {
        uint64_t position=(GetTickCount64()-started)*90;
        if(title && position>title->duration) position=title->duration;
        if(title && title->clip_count) {
            unsigned item=0;
            while(item+1<title->clip_count && title->clips[item+1].start_time<=position) ++item;
            bd_set_scr(bd,(int64_t)title->clips[item].in_time+position-title->clips[item].start_time);
        }
        BD_EVENT e={0}; int n=bd_read_ext(bd,buf,sizeof(buf),&e);
        if(n<0){s.errors++;break;}
        s.total_bytes+=n;
        do {
            if(e.event==BD_EVENT_PLAYLIST) {
                if(title) bd_free_title_info(title);
                title=bd_get_playlist_info(bd,e.param,0);started=GetTickCount64();
            }
            if(e.event!=BD_EVENT_IDLE) handle_event(&s,bd,e);
        }while(bd_get_event(bd,&e));
        if(!n || (title && bd_tell_time(bd)>position+9000)) Sleep(1);
    }
    if(title) bd_free_title_info(title);
    printf("{\"type\":\"summary\",\"played\":%d,\"draws\":%u,\"errors\":%u,\"frames\":%u}\n",played,draws,s.errors,s.ig_frames);
    bd_close(bd);for(unsigned i=0;i<3;i++)release_plane(&s.planes[i]);
    return s.errors?1:0;
}
