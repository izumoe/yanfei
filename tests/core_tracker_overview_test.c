/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_CORE 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void keys(uint32_t mask)
{
    host_notes |= mask & ~fm1_in.notes; fm1_in.notes = mask; keyboard_block(); frame();
}
static void combo(uint32_t modifiers, uint32_t key)
{
    keys(modifiers); keys(modifiers | (1u << key)); keys(modifiers); keys(0);
}
static void summaries(void)
{
    char a[12], b[12];
    step_t s = {.time=ST_NOTE, .n=1, .note={1}, .vel=96};
    set_engine_of(TSEL, 0); TSEL->engine = TSEL->eng_req;
    assert(tracker_summary(TSEL,&s,0,a,b)==1 && !strcmp(a,"C#-1"));
    s.n=2;s.note[1]=127;
    assert(tracker_summary(TSEL,&s,0,a,b)==2 && !strcmp(a,"C#-1") && !strcmp(b,"G9"));
    const struct {uint8_t n[4], count;const char *name;} chord[] = {
        {{0,4,7},3,""},{{0,3,7},3,"m"},{{0,3,6},3,"dim"},{{0,4,8},3,"+"},
        {{0,2,7},3,"sus2"},{{0,5,7},3,"sus4"},{{0,4,7,10},4,"7"},
        {{0,4,7,11},4,"M7"},{{0,3,7,10},4,"m7"},{{0,3,6,10},4,"m7b5"},
        {{0,3,6,9},4,"dim7"},{{0,3,7,11},4,"mM7"},{{0,4,7,9},4,"6"},{{0,3,7,9},4,"m6"}
    };
    for(uint32_t q=0;q<sizeof chord/sizeof chord[0];q++) for(uint32_t root=0;root<12;root++) {
        s.n=chord[q].count;
        for(uint32_t n=0;n<s.n;n++) s.note[n]=(uint8_t)(48+root+chord[q].n[n]);
        char want[12];snprintf(want,sizeof want,"%s%s",N_NOTE[root],chord[q].name);
        assert(tracker_summary(TSEL,&s,0,a,b)==1 && !strcmp(a,want));
        /* Permuting storage order must not change the root choice. */
        uint8_t temp=s.note[0];s.note[0]=s.note[s.n-1];s.note[s.n-1]=temp;
        assert(tracker_summary(TSEL,&s,0,a,b)==1 && !strcmp(a,want));
    }
    s=(step_t){.time=ST_NOTE,.n=3,.note={64,67,72}};
    assert(tracker_summary(TSEL,&s,0,a,b)==1 && !strcmp(a,"C"));
    s=(step_t){.time=ST_NOTE,.n=4,.note={60,64,67,72}};
    assert(tracker_summary(TSEL,&s,0,a,b)==1 && !strcmp(a,"C"));
    s.note[3]=61;assert(tracker_summary(TSEL,&s,0,a,b)==1 && !strcmp(a,"MULTI"));
    s.time=ST_TIE;tracker_summary(TSEL,&s,0,a,b);assert(!strcmp(a,"==="));
    s.time=ST_REST;tracker_summary(TSEL,&s,1,a,b);assert(!strcmp(a,"OFF"));
    tracker_summary(TSEL,&s,0,a,b);assert(!strcmp(a,"---"));
    set_engine_of(TSEL,ENGI_DRUM);TSEL->engine=TSEL->eng_req;
    s=(step_t){.time=ST_NOTE,.hit=1};tracker_summary(TSEL,&s,0,a,b);assert(!strcmp(a,"BD"));
    s.hit=3;assert(tracker_summary(TSEL,&s,0,a,b)==2 && !strcmp(a,"BD") && !strcmp(b,"SD"));
    s.hit=255;tracker_summary(TSEL,&s,0,a,b);assert(!strcmp(a,"MULTI"));
    puts("overview: dyads, all 12 roots, exact triads/sevenths, inversions, octave doubling, MULTI, drums and OFF/ties passed");
}
int main(void)
{
    ui_power_on();open_family(FAM_SEQ);frame();
    assert(ts_run.enabled && tracker.view==3 && tracker.section);tracker.section=0;summaries();
    set_engine_of(TSEL,0);TSEL->engine=TSEL->eng_req;
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==0 && tracker_on());
    go_home();open_family(FAM_SEQ);frame();assert(tracker.view==0);
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==3);
    tracker_move(5);combo(0,TK_RIGHT);assert(song.sel==1 && ui.cursor==5);
    combo(0,TK_LEFT);assert(song.sel==0 && ui.cursor==5);
    press(B_REC);assert(tracker.notes);
    keys((1u<<7)|(1u<<11)|(1u<<14));assert(TSEL->step[5].n==3);
    keys(0);assert(ui.cursor==6);
    tracker_move(5);step_t saved=TSEL->step[5];
    combo(1u<<TK_EDIT,TK_RIGHT);
    for(uint32_t n=0;n<saved.n;n++)assert(TSEL->step[5].note[n]==saved.note[n]+1);
    undo_swap();assert(!memcmp(&saved,&TSEL->step[5],sizeof saved));
    /* Group clamping must not flatten the chord's intervals at either end. */
    TSEL->step[5]=(step_t){.time=ST_NOTE,.n=2,.note={0,127},.vel=90};
    ts_bank.phrase[ts_bank.edit[0]].gate[5]=64;
    combo(1u<<TK_EDIT,TK_RIGHT);assert(TSEL->step[5].note[0]==0 && TSEL->step[5].note[1]==127);
    assert(ts_bank.phrase[ts_bank.edit[0]].gate[5]==64);
    TSEL->step[5]=saved;combo(1u<<TK_SHIFT,TK_OPTION);
    combo(0,TK_RIGHT);tracker_move(7);combo(1u<<TK_SHIFT,TK_EDIT);
    assert(!memcmp(&saved,&TSEL->step[7],sizeof saved));
    combo(1u<<TK_OPTION,TK_EDIT);assert(!step_on(&TSEL->step[7]));undo_swap();assert(step_on(&TSEL->step[7]));
    combo(0,TK_OFF);assert(ts_bank.phrase[ts_bank.edit[1]].off&(1u<<7));
    tracker_move(7);combo(1u<<TK_EDIT,TK_OFF);assert(TSEL->step[7].time==ST_TIE);
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==0);
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==3);
    set_engine_of(TSEL,ENGI_DRUM);TSEL->engine=TSEL->eng_req;tracker_move(9);
    uint32_t drum_keys=0, lanes=0;
    for(uint32_t k=5;k<=20 && __builtin_popcount(lanes)<3;k++) {
        uint32_t note=kb_map(TSEL,k);
        if(note!=KB_SILENT && !(lanes&(1u<<drum_lane(note)))) {
            lanes|=1u<<drum_lane(note);drum_keys|=1u<<k;
        }
    }
    assert(__builtin_popcount(lanes)==3);keys(drum_keys);keys(0);
    assert(TSEL->step[9].hit==lanes && ui.cursor==10);
    set_engine_of(TSEL,0);TSEL->engine=TSEL->eng_req;
    tracker_play(0);assert(ts_run.pending==TS_ALL);seq_start();assert(ts_run.active==15);seq_stop();transport_req=0;
    tracker.notes=0;song.rec=0;tracker_publish();ui.msg_t=0;ui.force=1;frame();frame();
    host_blit_rows=0;frame();assert(host_blit_rows==0);
    tracker_move(3);host_blit_rows=0;frame();assert(host_blit_rows<=40);
    TSEL->step[3]=(step_t){.time=ST_NOTE,.n=1,.note={60},.vel=96};frame();
    host_blit_rows=0;TSEL->step[3].vel=100;frame();assert(host_blit_rows==28);
    host_blit_rows=0;trk[2].step[11].vel=99;frame();assert(host_blit_rows==12);
    assert(!project_save_as(0,"OVERVIEW"));project_load(0);assert(ts_run.enabled);
    puts("overview: actual keys enter chords, preserve rows across tracks, transpose, copy/paste, clear/undo, OFF/tie, switch detail, play all four, dirty refresh and save passed");
}
