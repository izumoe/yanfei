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
static void boot(void)
{
    ui_power_on(); open_family(FAM_SEQ); frame();
    assert(ts_run.enabled && tracker.view == 3 && tracker.section && !tracker.page);
}
static void scroll_and_edit(void)
{
    boot(); uint32_t used = ts_bank.phrases;
    for (uint32_t r = 1; r < 64; r++) {
        combo(0, TK_DOWN); assert(tracker.page == r / 16 && ui.cursor == r % 16);
    }
    combo(0,TK_DOWN);assert(tracker.page==3 && ui.cursor==15);
    assert(used == ts_bank.phrases); /* Browsing and preview do not allocate. */
    tracker_preview();assert(used == ts_bank.phrases);
    combo(1u<<TK_OPTION,TK_EDIT);assert(used == ts_bank.phrases);
    combo(0,TK_EDIT);uint32_t p=tracker_section_phrase(0);
    assert(p < TS_PHRASES && trk[0].step[15].n==1);
    assert(!ts_sync() && ts_bank.phrase[p].step[15].n==1);
    tracker_move(-33);assert(tracker.page==0 && ui.cursor==15);
    combo(1u<<TK_OPTION,TK_DOWN);assert(tracker.page==1 && ui.cursor==15);
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==0 && tracker.page==1);
    /* SELECT and its modifiers go through the real input handler. */
    turn(EN_SELECT,1);assert(tracker.page==2 && ui.cursor==0);
    keys(1u<<TK_SHIFT);turn(EN_SELECT,-1);keys(0);assert(tracker.page==1 && ui.cursor==0);
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==3);
    tracker_move(32);assert(tracker.page==3 && ui.cursor==0);
    press(B_REC);keys(1u<<7);keys(0);assert(ui.cursor==1 && TSEL->step[0].n==1);
    uint32_t before=ts_bank.phrases;tracker_move(0);tracker_copy();
    combo(0,TK_RIGHT);tracker_move(3);tracker_paste();
    assert(tracker_section_phrase(1)<TS_PHRASES && TSEL->step[3].n==1);
    assert(ts_bank.phrases!=before && !ts_sync());
    tracker_move(-48);assert(!tracker.page);
    keys((1u<<TK_SHIFT)|(1u<<TK_OPTION));turn(EN_SELECT,-2);keys(0);
    assert(ts_section[0].beats==8 && tracker_section_pages()==2);
    keys((1u<<TK_SHIFT)|(1u<<TK_OPTION));turn(EN_SELECT,20);keys(0);
    assert(ts_section[0].beats==16 && tracker_section_pages()==4);
    tracker_section_length(-9);assert(!ts_section[0].beats);
    tracker_section_length(1);assert(ts_section[0].beats==4);
    tracker_section_length(3);assert(ts_section[0].beats==16);
    tracker_move(48);assert(tracker.page==3);tracker_history_swap();
    assert(ts_section[0].beats==4 && !tracker.page);tracker_history_swap();assert(ts_section[0].beats==16);
    keys(1u<<TK_OPTION);turn(EN_PRESET,-1);keys(0);assert(!(ts_section[0].loop&2));
    tracker_history_swap();assert(ts_section[0].loop&2);
    assert(!project_save_as(0,"FOUR BARS"));project_load(0);
    assert(ts_section[0].beats==16 && ts_section[0].loop==15);
    puts("section: all 64 rows, clamped scroll, blank-page reads, note entry, row paste, page turns, four-bar limit, repeat controls and save passed");
}
static void shared_pages(void)
{
    boot();combo(0,TK_EDIT);assert(!ts_sync());
    assert(motion_set_lock(TSEL,2,P_LEVEL,60)!=2);motion_set_enabled(TSEL,1);assert(!ts_sync());
    uint32_t source=tracker_section_phrase(0);
    combo((1u<<TK_SHIFT)|(1u<<TK_OPTION),TK_LEFT);
    combo(1u<<TK_OPTION,TK_DOWN);
    combo((1u<<TK_SHIFT)|(1u<<TK_OPTION),TK_RIGHT);
    assert(tracker_section_phrase(0)==source && TSEL->step[0].n==1);
    combo((1u<<TK_SHIFT)|(1u<<TK_OPTION),TK_EDIT);
    uint32_t copy=tracker_section_phrase(0);
    assert(copy!=source && copy<TS_PHRASES && ts_bank.count==2);
    int16_t value;assert(motion_lock_get(TSEL,2,P_LEVEL,&value) && value==60);
    tracker_cell(12,0,0);assert(!ts_sync());
    assert(ts_bank.phrase[copy].step[0].note[0]==ts_bank.phrase[source].step[0].note[0]+12);
    tracker_history_swap();assert(tracker_section_phrase(0)==source);
    tracker_history_swap();assert(tracker_section_phrase(0)==copy);
    tracker.view=2;tracker.song_col=0;combo(1u<<TK_SHIFT,TK_OPTION);
    tracker.song_row=1;combo(1u<<TK_SHIFT,TK_EDIT);
    assert(ts_section[1].beats==16 && ts_section[1].loop==15);
    tracker_history_swap();assert(ts_section[1].beats==0 && ts_bank.order[1][0]==TS_NONE);
    tracker_history_swap();assert(ts_section[1].beats==16 && ts_bank.order[1][0]==0);
    combo(1u<<TK_SHIFT,TK_UP);assert(tracker.view==3 && tracker.section && !tracker.page && tracker.song_row==1);
    puts("section: shared page paste, independent clone with locks, undo/redo and whole-section metadata copy passed");
}
/* Run the real transport and per-track scheduler, in audio block units. */
static void tick(uint32_t n) { events_block(n); }
static void set_phrase(uint32_t id,uint32_t length,uint32_t note)
{
    ts_phrase_t *p=&ts_bank.phrase[id];p->length=(uint8_t)length;
    p->step[0]=(step_t){.time=ST_NOTE,.n=1,.note={(uint8_t)note},.vel=96};
    if(ts_bank.edit[0]==id){memcpy(trk[0].step,p->step,sizeof p->step);trk[0].p[P_SLEN]=length;ts_run.cache[0]=ts_cache_hash(0);}
}
static void playback(void)
{
    boot();song.rec=0;host_notes=fm1_in.notes=0;
    memset(ts_bank.order,TS_NONE,sizeof ts_bank.order);memset(ts_section,0,sizeof ts_section);
    for(uint32_t c=0;c<TS_CHAINS;c++) for(uint32_t r=0;r<TS_ROWS;r++)ts_bank.chain[c][r]=(ts_link_t){TS_NONE,0};
    ts_bank.order[0][0]=0;ts_bank.order[0][1]=1;ts_bank.order[1][0]=2;ts_bank.song_length=2;
    ts_bank.chain[0][0]=(ts_link_t){0,12};ts_bank.chain[0][1]=(ts_link_t){1,0};
    ts_bank.chain[1][0]=(ts_link_t){4,0};ts_bank.chain[2][0]=(ts_link_t){8,0};
    set_phrase(0,8,48);set_phrase(1,16,50);set_phrase(4,4,52);set_phrase(8,16,60);
    ts_section[0]=(ts_section_t){16,1};ts_section[1]=(ts_section_t){4,0};
    trk[0].p[P_SDIV]=2;trk[1].p[P_SDIV]=3;trk[1].p[P_SSWING]=70;
    assert(!ts_prepare(TS_SONG,0,0,0));seq_start();tick(128);
    assert(ts_run.transpose[0]==12 && trk[0].seq_notes[0]==60);
    uint32_t duration=div_samples(2)*64,elapsed=0,repeat=0,wait=0,last=ts_run.link[0];
    while(elapsed<duration){uint32_t n=duration-elapsed>128?128:duration-elapsed;tick(n);elapsed+=n;
        if(elapsed<duration){assert(song.playing && ts_run.order[0]==0);wait|=ts_run.waiting&2;
            if(last==1 && ts_run.link[0]==0)repeat++;last=ts_run.link[0];}}
    assert(repeat>=2 && wait && song.playing && ts_run.order[0]==1 && !ts_run.waiting && ts_run.clock_row==0);
    assert(trk[0].seq_idx==0 && trk[0].seq_notes[0]==60 && !trk[1].seq_n);
    duration=div_samples(2)*16;elapsed=0;
    while(elapsed<duration){uint32_t n=duration-elapsed>128?128:duration-elapsed;tick(n);elapsed+=n;}
    assert(!song.playing && !ts_run.mode);
    /* Four-bar looping uses the same boundary, including a completely silent section. */
    ts_section[1]=(ts_section_t){4,15};ts_bank.order[1][0]=TS_NONE;
    assert(!ts_prepare(TS_SECTION,0,0,1));seq_start();tick(128);
    for(uint32_t q=0;q<2;q++){elapsed=0;while(elapsed<duration){uint32_t n=duration-elapsed>128?128:duration-elapsed;tick(n);elapsed+=n;}
        assert(song.playing && ts_run.mode==TS_SECTION && ts_run.order[0]==1 && !ts_run.active && !ts_run.clock_row);}
    seq_stop();
    /* An internal missing page consumes one silent bar instead of being skipped. */
    ts_bank.order[0][0]=0;ts_bank.chain[0][0]=(ts_link_t){0,0};ts_bank.chain[0][1]=(ts_link_t){TS_NONE,0};ts_bank.chain[0][2]=(ts_link_t){1,0};
    set_phrase(0,16,48);ts_section[0]=(ts_section_t){16,0};
    assert(!ts_prepare(TS_SECTION,0,0,0));seq_start();tick(128);
    duration=div_samples(2)*16;
    for(uint32_t q=0;q<2;q++){elapsed=0;while(elapsed<duration){uint32_t n=duration-elapsed>128?128:duration-elapsed;tick(n);elapsed+=n;}
        assert(ts_run.link[0]==q+1);if(!q)assert((ts_run.gap&1) && !step_on(seq_steps(&trk[0])));else assert(!(ts_run.gap&1) && ts_run.phrase[0]==1);}
    seq_stop();
    puts("section: exact 64-sixteenth boundary, different lengths/divisions/swing, repeat/wait, first-link transpose, HOLD release, empty section loop and silent page gaps passed");
}
static void clock_edges(void)
{
    boot();song.rec=0;host_notes=fm1_in.notes=0;song.g[G_BPM]=131;
    ts_section[0]=(ts_section_t){4,15};
    assert(!ts_prepare(TS_SECTION,0,0,0));seq_start();tick(128);
    uint32_t duration=div_samples(2)*16,total=0,previous=0,boundaries=0;
    while(total<duration*100u){tick(128);total+=128;
        uint32_t section=total/duration;
        if(section!=previous){assert(ts_run.clock_row==0 && ts_run.clock_pos==total%duration);
            assert(trk[0].seq_idx==0 && trk[0].seq_pos==total%duration);boundaries++;previous=section;}}
    assert(boundaries==100 && song.playing);seq_stop();
    boot();song.rec=0;host_notes=fm1_in.notes=0;
    ts_section[0]=(ts_section_t){4,15};
    trk[0].p[P_SDIV]=2;trk[0].p[P_SSWING]=0;
    assert(!ts_prepare(TS_SECTION,0,0,0));song.g[G_CLOCK]=midi_clock.mode=1;
    midi_clock_transport(0xFA,fm1_ms);tick(128);
    uint32_t first=fm1_ms;
    for(uint32_t pulse=0;pulse<=96;pulse++){
        fm1_ms=first+pulse*21u;midi_clock_pulse(fm1_ms);tick(128);
        assert(song.playing && ts_run.clock_pos==trk[0].seq_pos);
        assert(ts_run.clock_row%16u==trk[0].seq_idx);
    }
    uint32_t pos=ts_run.clock_pos,row=ts_run.clock_row;
    tick(128);assert(ts_run.clock_pos==pos && ts_run.clock_row==row);
    fm1_ms+=501;tick(128);assert(!song.playing && !ts_run.mode);
    for(uint32_t k=0;k<NTRK;k++)assert(!trk[k].seq_n);
    puts("section: 100 fixed-block loops preserve fractional carry; MIDI tempo phase, pulse pause and cable-loss release passed");
}
static void old_project(void)
{
    boot();FILE *f=fopen("tests/fixtures/yanfei-y12-project.bin","rb");assert(f);
    assert(fread(&ts_disk,1,TS_DISK_V1_BYTES,f)==TS_DISK_V1_BYTES);assert(fgetc(f)==EOF);fclose(f);
    assert(TS_DISK_V1_BYTES==15728 && ts_disk_valid() && !ts_apply_disk());
    assert(!strcmp(proj_name,"Y12 FIXTURE") && ts_run.enabled && ts_bank.song_length==2);
    assert(ts_bank.phrase[0].step[0].n==3 && ts_bank.phrase[0].gate[0]==48 && ts_bank.phrase[0].off==2);
    for(uint32_t s=0;s<TS_SONG_ROWS;s++)assert(!ts_section[s].beats);
    assert(!ts_store_write(1,-1,1,TS_DISK_V1_BYTES));project_load(1);assert(!ts_section[0].beats);
    assert(!project_rename(1,"OLD RENAMED"));ts_head_t h;assert(ts_store_current(1,&h)>=0 && h.length==sizeof ts_disk && ts_disk.version==2);
    project_load(1);assert(ts_run.enabled && !ts_section[0].beats && ts_bank.phrase[0].gate[0]==48);
    for(uint32_t bad=0;bad<2;bad++) {
        /* Replace the newer copy with a semantically invalid but CRC-correct one. */
        ts_upgrade_disk();ts_disk.section[0]=(ts_section_t){bad?16:17,bad?16:15};
        assert(!ts_store_write(1,0,3,sizeof ts_disk));project_load(1);
        assert(ts_run.enabled && !ts_section[0].beats && !strcmp(proj_name,"Y12 FIXTURE"));
    }
    ts_section[0]=(ts_section_t){16,15};assert(!project_save(1));project_load(1);assert(ts_section[0].beats==16);
    puts("section: unchanged y12 fixture imports with AUTO timing, journal load, rename upgrade and new save roundtrip passed");
}
int main(void){scroll_and_edit();shared_pages();playback();clock_edges();old_project();}
