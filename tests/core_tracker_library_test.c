/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_CORE 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void library_boot(void)
{
    ui_power_on();
    tracker.section = 0;
    tracker.view = 0; /* These tests exercise the detailed phrase controls. */
    open_family(FAM_SEQ); frame();
    assert(ts_run.enabled && tracker_on());
}
static void keys(uint32_t mask)
{
    host_notes |= mask & ~fm1_in.notes; fm1_in.notes = mask; keyboard_block(); frame();
}
static void combo(uint32_t modifiers, uint32_t key)
{
    keys(modifiers); keys(modifiers | (1u << key)); keys(modifiers); keys(0);
}
static void migration(void)
{
    ui_power_on();
    step_t before[NTRK][NSTEP];
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = 64;
        for (uint32_t r = 0; r < NSTEP; r++) {
            step_t *s = &trk[k].step[r]; s->time = r % 3u; s->n = s->time == ST_NOTE ? 4 : 0;
            for (uint32_t n = 0; n < 4; n++) s->note[n] = (uint8_t)(36u + k * 12u + n + r % 12u);
            s->hit = r & 1u ? 255 : 0; s->acc = s->hit & 85u;
            step_set_nudge(s, (int32_t)(r % 16u) - 8); step_set_ratchet(s, 1 + r % 4u);
            step_set_chance_v(s, r % 112u); s->vel = 96;
        }
        memcpy(before[k], trk[k].step, sizeof before[k]);
        assert(motion_set_lock(&trk[k], 63, P_LEVEL, 72) != 2); motion_set_enabled(&trk[k], 1);
    }
    open_family(FAM_SEQ); frame();
    assert(ts_run.enabled && ts_bank.count == 4);
    for (uint32_t k = 0; k < NTRK; k++) for (uint32_t page = 0; page < 4; page++) {
        const ts_phrase_t *p = &ts_bank.phrase[k * 4u + page];
        assert(!memcmp(p->step, before[k] + page * TS_ROWS, sizeof p->step));
        assert(ts_bank.chain[k][page].phrase == k * 4u + page);
        assert(p->gate[0] == trk[k].p[P_SGATE]);
    }
    assert(ts_select_phrase(0, 3) == 0);
    int16_t value; assert(motion_lock_get(&trk[0], 15, P_LEVEL, &value) && value == 72);
    assert(ts_sync() == 0 && ts_bank.count == 4);
    puts("tracker: all 256 legacy rows, chords, drums, timing and automation migrated");
}
static void sharing_and_clone(void)
{
    library_boot();
    assert(!ts_select_phrase(1, 0));
    trk[0].step[0].n = 1; trk[0].step[0].time = ST_NOTE; trk[0].step[0].note[0] = 60;
    assert(!ts_sync()); assert(trk[1].step[0].note[0] == 60 && trk[1].step[0].n == 1);
    assert(motion_set_lock(&trk[0], 2, P_LEVEL, 55) != 2); motion_set_enabled(&trk[0], 1);
    assert(!ts_sync()); int16_t value;
    assert(motion_lock_get(&trk[1], 2, P_LEVEL, &value) && value == 55 && ts_bank.count == 1);
    tracker.view = 1; tracker.chain = tracker.chain_row = 0;
    combo((1u << TK_SHIFT) | (1u << TK_OPTION), TK_EDIT);
    uint32_t cloned = ts_bank.chain[0][0].phrase;
    assert(cloned >= 16 && cloned < TS_PHRASES);
    assert(!memcmp(&ts_bank.phrase[cloned], &ts_bank.phrase[0], sizeof(ts_phrase_t)));
    assert(ts_bank.count == 2 && ts_bank.event[1].phrase == cloned);
    tracker_history_swap(); assert(ts_bank.chain[0][0].phrase == 0);
    tracker_history_swap(); assert(ts_bank.chain[0][0].phrase == cloned);
    assert(!ts_select_phrase(0, cloned)); trk[0].step[0].note[0] = 72; assert(!ts_sync());
    assert(ts_bank.phrase[0].step[0].note[0] == 60 && ts_bank.phrase[cloned].step[0].note[0] == 72);
    combo(1u << TK_SHIFT, TK_OPTION); tracker.chain_row = 1; combo(1u << TK_SHIFT, TK_EDIT);
    assert(ts_bank.chain[0][1].phrase == cloned);
    tracker.view = 2; tracker.song_row = tracker.song_col = 0;
    uint8_t section[NTRK];memcpy(section,ts_bank.order[0],NTRK);
    combo(1u << TK_SHIFT,TK_OPTION); tracker.song_row=2;combo(1u << TK_SHIFT,TK_EDIT);
    assert(!memcmp(ts_bank.order[2],section,NTRK));tracker_history_swap();
    for(uint32_t k=0;k<NTRK;k++)assert(ts_bank.order[2][k]==TS_NONE);
    tracker_history_swap();assert(!memcmp(ts_bank.order[2],section,NTRK));tracker.song_row=0;
    combo((1u << TK_SHIFT) | (1u << TK_OPTION), TK_EDIT);
    uint32_t chain_id = ts_bank.order[0][0]; assert(chain_id >= 4 && chain_id < TS_CHAINS);
    assert(!memcmp(ts_bank.chain[chain_id], ts_bank.chain[0], sizeof ts_bank.chain[0]));
    ts_bank.chain[chain_id][0].transpose = 12; assert(ts_bank.chain[0][0].transpose == 0);
    assert(!project_save_as(1, "CLONES"));
    puts("tracker: shared edits and automation, phrase/chain clone, references and undo/redo passed");
}
static void note_behavior(void)
{
    library_boot(); assert(!ts_select_phrase(0, 16));
    tracker.col = 0; combo(0, TK_EDIT); assert(TSEL->step[0].n == 1);
    ts_phrase_t *p = &ts_bank.phrase[16]; assert(p->gate[0] == 0);
    tracker_gate(64); assert(p->gate[0] == 64); undo_swap(); assert(p->gate[0] == 0); undo_swap(); assert(p->gate[0] == 64);
    tracker_copy(); tracker_move(3); tracker_paste(); assert(p->gate[3] == 64 && TSEL->step[3].n == 1);
    undo_swap(); assert(TSEL->step[3].time == ST_REST && p->gate[3] == 0); undo_swap(); assert(p->gate[3] == 64);
    assert(!ts_sync()); assert(!ts_prepare(TS_PHRASE, 0, 0, 0)); seq_start();
    p->gate[0] = 0; seq_play(TSEL, 0, 1000); assert(TSEL->seq_n == 1 && TSEL->seq_hold);
    seq_play(TSEL, 1, 1000); assert(TSEL->seq_n == 1 && TSEL->seq_hold);
    seq_play(TSEL, 0, 1000); assert(!TSEL->slide_glide);
    p->off |= 1u << 2; seq_play(TSEL, 2, 1000); assert(!TSEL->seq_n);
    p->gate[0] = 32; seq_play(TSEL, 0, 1000); assert(TSEL->seq_off == 250 && !TSEL->seq_hold);
    seq_stop();
    tracker.notes = 1; tracker_move(4); combo(0, TK_OFF); assert(p->off & (1u << 4));
    assert(!project_save(2)); project_load(2); assert(ts_bank.phrase[16].gate[0] == 32 && (ts_bank.phrase[16].off & (1u << 4)));
    puts("tracker: HOLD, blank continuation, explicit OFF, per-row gate, metadata clipboard/undo/save passed");
}
static void arrangement(void)
{
    library_boot(); memset(ts_section, 0, sizeof ts_section); memset(ts_bank.order, TS_NONE, sizeof ts_bank.order);
    for (uint32_t c = 0; c < TS_CHAINS; c++) for (uint32_t r = 0; r < TS_ROWS; r++) ts_bank.chain[c][r] = (ts_link_t){TS_NONE, 0};
    ts_bank.chain[0][0] = (ts_link_t){0, 12}; ts_bank.chain[0][1] = (ts_link_t){1, -12};
    ts_bank.chain[1][0] = (ts_link_t){4, 0}; ts_bank.chain[2][0] = (ts_link_t){8, 0};
    ts_bank.order[0][0] = 0; ts_bank.order[0][1] = 1; ts_bank.order[1][0] = 2; ts_bank.song_length = 2;
    assert(!ts_prepare(TS_SONG, 0, 0, 0)); seq_start(); assert(ts_run.active == 3 && ts_run.phrase[0] == 0);
    assert(!ts_advance(1)); assert(ts_run.waiting == 2 && ts_run.order[1] == 0);
    assert(ts_advance(0)); assert(ts_run.phrase[0] == 1 && ts_run.transpose[0] == -12 && !ts_run.cycle[0]);
    assert(!seq_steps(TSEL)[63].n && !seq_steps(TSEL)[63].hit);
    assert(ts_advance(0)); assert(ts_run.order[0] == 1 && ts_run.active == 1 && !ts_run.waiting && ts_run.phrase[0] == 8);
    assert(!ts_advance(0) && !song.playing && !ts_run.mode);
    assert(!ts_prepare(TS_CHAIN, 0, 0, 0)); seq_start(); assert(ts_run.phrase[0] == 0);
    assert(ts_advance(0) && ts_run.phrase[0] == 1); assert(ts_advance(0) && ts_run.phrase[0] == 0 && ts_run.cycle[0]); seq_stop();
    /* Actual sequencer pitch output, not just the transpose metadata. */
    TSEL->step[0] = (step_t){.note={60},.n=1,.time=ST_NOTE,.vel=96}; assert(!ts_sync());
    assert(!ts_prepare(TS_CHAIN,0,0,0)); seq_start(); seq_play(TSEL,0,1000); assert(TSEL->seq_notes[0] == 72); seq_stop();
    open_family(FAM_GLO); assert(tracker_on() && tracker.view == 2);
    puts("tracker: phrase/chain/song transport, section barriers, transpose, chain cycles and GLO navigation passed");
}
static void library_soak(void)
{
    library_boot(); uint32_t random=174325u;
    for(uint32_t i=0;i<2000;i++) {
        random^=random<<13;random^=random>>17;random^=random<<5;
        tracker.view=(uint8_t)(1u+(random&1u));tracker.chain=(uint8_t)((random>>4)%TS_CHAINS);
        tracker.chain_row=(uint8_t)((random>>8)%TS_ROWS);tracker.table_col=(uint8_t)((random>>13)&1u);
        tracker.song_row=(uint8_t)((random>>16)%TS_SONG_ROWS);tracker.song_col=(uint8_t)((random>>21)%NTRK);
        switch(random%5u) {
        case 0:tracker_library_cell(random&32u?1:-1,0);break;
        case 1:tracker_library_cell(0,1);break;
        case 2:tracker_clone();break;
        case 3:tracker_history_swap();break;
        default:tracker_library_cell(0,0);break;
        }
        assert(ts_bank.count<=MOTION_MAX && ts_bank.song_length<=TS_SONG_ROWS);
        if(i%100u==0) assert(!project_save(3));
    }
    assert(!project_save(3));project_load(3);assert(ts_run.enabled);
    puts("tracker: 2,000 library reference/transpose/clone/clear/undo operations remain saveable");
}
int main(void)
{
    migration(); sharing_and_clone(); note_behavior(); arrangement(); library_soak();
    library_boot();
    printf("tracker bank %zu, disk %zu bytes\n", sizeof ts_bank, sizeof ts_disk);
    assert(ts_sync() == 0);
    assert(project_save_as(0, "TRACKER") == 0);
    assert(ts_disk_valid());
    ts_bank.phrase[0].step[0].note[0] = 72;
    project_load(0);
    assert(ts_run.enabled && ts_bank.phrase[0].step[0].note[0] != 72);
    assert(!project_rename(0,"RENAMED")); assert(!strcmp(proj_name,"RENAMED"));
    for (uint32_t en=0; en<NENG_SHOWN; en++) for (uint32_t pr=0; pr<ENGINES[eng_vis(en)]->npresets; pr++) {
        set_engine_of(TSEL,eng_vis(en)); apply_preset_to(TSEL,pr);
        assert(!project_save(3));
    }
    assert(!project_erase(0)); assert(!project_used(0));
    puts("tracker library: activation and full project roundtrip passed");
}
