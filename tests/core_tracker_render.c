/* SPDX-License-Identifier: GPL-3.0-only */
/* Reuse the real screen's ink/overlap/contrast lint for every tracker column. */
#define FELUCCA_CORE 1
#define FELUCCA_VERSION "v1.6.1-y13"
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"
int main(void)
{
    uint32_t scenes = 0;
    const char *report = getenv("TRACKER_LAYOUT_REPORT");
    rep = fopen(report ? report : "build/tracker-layout.txt", "w");
    if (!rep) return 2;
    for (uint32_t p=0;p<NPALETTES;p++) for (uint32_t mode=0;mode<4;mode++) {
        static const int page[] = {S_SPLASH,S_ABOUT,S_ABOUT_CREDITS,S_ABOUT_END};
        for(uint32_t i=0;i<sizeof page/sizeof page[0];i++) {
            char name[80];snprintf(name,sizeof name,"%s/brand/%s/mode%u",UI_PALETTES[p].name,S_NAME[page[i]],mode);
            cur_name=name;setup(page[i]);pal(p);ui_style=mode&1u;ui_prefs=mode&2u?PREF_LARGE:0;
            draw(page[i]);lint();mono_check();scenes++;
            if(p==UI_GREY_INDEX && !mode && getenv("BRAND_RENDER_DIR"))write_ppm(getenv("BRAND_RENDER_DIR"),"GREY",S_NAME[page[i]]);
        }
    }
    for (uint32_t p = 0; p < NPALETTES; p++)
    for (uint32_t mode = 0; mode < 4; mode++)
    for (uint32_t en = 0; en < NENG_SHOWN; en++) {
        ui_power_on(); memset(&tracker, 0, sizeof tracker);
        pal(p); ui_style = (uint8_t)(mode & 1u); ui_prefs = mode & 2u ? PREF_LARGE : 0u;
        set_engine_of(TSEL, eng_vis(en)); TSEL->engine = TSEL->eng_req;
        open_family(FAM_SEQ); TSEL->p[P_SLEN] = 16;
        for (uint32_t row = 0; row < NSTEP; row++) {
            step_t *s = &TSEL->step[row]; memset(s, 0, sizeof *s);
            s->time = row % 4u == 1u ? ST_TIE : row % 4u == 2u ? ST_REST : ST_NOTE;
            s->vel = row & 1u ? 127 : 1;
            step_set_chance_v(s, 100u + row % (NCOND + 1u));
            step_set_ratchet(s, 1u + row % 4u);
            if (drum_track(TSEL)) s->hit = s->acc = 255;
            else { s->n = 4; s->note[0] = 1; s->note[1] = 13; s->note[2] = 126; s->note[3] = 127; }
        }
        for (uint32_t col = 0; col < tracker_ncol(); col++) {
            char name[80]; snprintf(name, sizeof name, "%s/%s/mode%u/col%u", UI_PALETTES[p].name, ENGINES[TSEL->engine]->name, mode, col);
            cur_name = name; tracker.col = (uint8_t)col; ui.cursor = (uint8_t)col; ui.bank = 0;
            ui.msg_t = 0; ui.force = 1; nscr = npend = nrules = 0;
            ui_draw(); lint(); mono_check(); scenes++;
        }
        for (uint32_t detail=0; detail<4; detail++) {
            char name[80];snprintf(name,sizeof name,"%s/%s/footer%u/mode%u",UI_PALETTES[p].name,ENGINES[TSEL->engine]->name,detail,mode);
            step_t *s=&TSEL->step[0];s->time=detail==3u?ST_REST:ST_NOTE;s->vel=127;
            s->flags=detail==2u?SF_ACCENT:0;step_set_nudge(s,detail&1u?7:-8);step_set_ratchet(s,detail==1u?4:1);
            step_set_chance_v(s,detail==0u?CHANCE_V_MAX:detail==1u?100u:detail==2u?CHANCE_V_MAX-1u:0u);
            TSEL->p[P_SQNT]=detail>=2u;cur_name=name;ui.cursor=tracker.col=0;
            ui.msg_t=0;ui.force=1;nscr=npend=nrules=0;ui_draw();lint();mono_check();scenes++;
        }
        for (uint32_t part=0;part<NTRK;part++) {
            tracker.view=3;song.sel=(uint8_t)part;ui.cursor=15;
            for(uint32_t k=0;k<NTRK;k++) for(uint32_t r=0;r<TS_ROWS;r++) {
                step_t *s=&trk[k].step[r];
                static const uint8_t tones[14][4] = {
                    {0,4,7,0},{0,3,7,0},{0,3,6,0},{0,4,8,0},{0,2,7,0},{0,5,7,0},
                    {0,4,7,10},{0,4,7,11},{0,3,7,10},{0,3,6,10},{0,3,6,9},
                    {0,3,7,11},{0,4,7,9},{0,3,7,9}};
                *s=(step_t){.time=ST_NOTE,.n=2,.note={1,3},.vel=127};
                if(r>0u && r<15u) {
                    s->n=r<=6u?3:4;
                    for(uint32_t n=0;n<s->n;n++)s->note[n]=61+tones[r-1u][n];
                }
                if(r==15u)s->hit=255;
            }
            char name[80];snprintf(name,sizeof name,"%s/overview%u/%s/mode%u",UI_PALETTES[p].name,part,ENGINES[TSEL->engine]->name,mode);
            cur_name=name;ui.msg_t=0;ui.force=1;nscr=npend=nrules=0;ui_draw();lint();mono_check();scenes++;
        }
        song.sel=0;
        for (uint32_t view=1; view<=2; view++) for (uint32_t bank=0; bank<(view==2?2u:1u); bank++) {
            tracker.view=(uint8_t)view; tracker.song_row=(uint8_t)(bank*16u+15u); tracker.chain_row=15;
            tracker.song_col=3;tracker.table_col=1;
            for(uint32_t r=0;r<TS_SONG_ROWS;r++) for(uint32_t k=0;k<NTRK;k++) ts_bank.order[r][k]=(uint8_t)((r+k)%TS_CHAINS);
            for(uint32_t r=0;r<TS_ROWS;r++) ts_bank.chain[tracker.chain][r]=(ts_link_t){31,(int8_t)(r&1u?-24:24)};
            char name[80];snprintf(name,sizeof name,"%s/library%u/bank%u/mode%u",UI_PALETTES[p].name,view,bank,mode);
            cur_name=name;ui.msg_t=0;ui.force=1;nscr=npend=nrules=0;ui_draw();lint();mono_check();scenes++;
        }
        /* All four scrolling pages, empty tracks/pages and R/W labels. */
        tracker.song_row=0;tracker.section=1;ts_section[0]=(ts_section_t){16,5};
        memcpy(ts_bank.phrase[31].step,trk[0].step,sizeof ts_bank.phrase[31].step);
        ts_bank.phrase[31].off=4;ts_bank.phrase[31].length=16;
        for(uint32_t k=0;k<NTRK;k++) {
            ts_bank.order[0][k]=k==3?TS_NONE:k;
            for(uint32_t pg=0;pg<4;pg++) ts_bank.chain[k][pg]=(ts_link_t){pg==k?TS_NONE:31,0};
        }
        for(uint32_t view=0;view<2;view++)for(uint32_t pg=0;pg<4;pg++) {
            tracker.view=view?3:0;tracker.page=(uint8_t)pg;ui.cursor=15;ui.bank=0;
            char name[80];snprintf(name,sizeof name,"%s/section%u/page%u/%s/mode%u",UI_PALETTES[p].name,view,pg,ENGINES[TSEL->engine]->name,mode);
            cur_name=name;ui.msg_t=0;ui.force=1;nscr=npend=nrules=0;ui_draw();lint();mono_check();scenes++;
        }
    }
    fclose(rep);
    printf("tracker layout: %u scenes across %u palettes, 12 engines, all columns/styles/LARGE; %u findings, %u invisible inks, %u off-gray pixels\n",
           scenes, NPALETTES, nfind, nink, mono_bad);
    return nfind || nink || mono_bad ? 1 : 0;
}
