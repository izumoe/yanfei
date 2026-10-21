/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
static int tracker_test_prog(uint32_t, const void *, uint32_t);
static int tracker_test_erase(uint32_t);
#define FELUCCA_CORE 1
#define TS_PROG tracker_test_prog
#define TS_ERASE tracker_test_erase
#include "../web/emu/felucca_web.c"
#include <assert.h>

static int write_left = -1, erase_left = -1;
static int tracker_test_prog(uint32_t off, const void *src, uint32_t n)
{
    uint32_t allowed = write_left < 0 || (uint32_t)write_left >= n ? n : (uint32_t)write_left;
    if (allowed && st_prog(off,src,allowed)) return -1;
    if (write_left >= 0) write_left -= (int)allowed;
    return allowed == n ? 0 : -1;
}
static int tracker_test_erase(uint32_t off)
{
    if (!erase_left) return -1;
    if (erase_left > 0) erase_left--;
    return st_erase(off);
}
int main(void)
{
    web_nor_erase(); web_boot(); assert(!ts_activate());
    trk[0].step[0] = (step_t){.note={60},.n=1,.time=ST_NOTE,.vel=96};
    assert(!project_save_as(0,"OLD"));
    static uint8_t baseline[sizeof nor]; memcpy(baseline,nor,sizeof nor);
    uint32_t before_low = ts_crc(nor,TS_STORE_BASE), before_high = ts_crc(nor+0xC8000,sizeof nor-0xC8000);
    uint32_t cases = 0;
    for (uint32_t cut=0; cut<=sizeof ts_disk+sizeof(ts_head_t); cut = cut < sizeof ts_disk ? (cut + 127u > sizeof ts_disk ? sizeof ts_disk : cut + 127u) : cut + 1u) {
        memcpy(nor,baseline,sizeof nor); project_load(0); trk[0].step[0].note[0] = 72;
        write_left = (int)cut; (void)project_save_as(0,"NEW"); write_left = -1;
        project_load(0); assert(ts_run.enabled);
        uint32_t note = trk[0].step[0].note[0]; assert(note == 60 || note == 72);
        assert((note == 60 && !strcmp(proj_name,"OLD")) || (note == 72 && !strcmp(proj_name,"NEW")));
        assert(ts_crc(nor,TS_STORE_BASE) == before_low && ts_crc(nor+0xC8000,sizeof nor-0xC8000) == before_high); cases++;
    }
    for (uint32_t cut=0; cut<4; cut++) {
        memcpy(nor,baseline,sizeof nor); project_load(0); trk[0].step[0].note[0] = 72;
        erase_left = (int)cut; assert(project_save_as(0,"NEW") != 0); erase_left = -1;
        project_load(0); assert(trk[0].step[0].note[0] == 60); cases++;
    }
    memcpy(nor,baseline,sizeof nor); project_load(0); trk[0].step[0].note[0] = 72;
    assert(!project_save_as(0,"NEW")); ts_head_t winner; int copy = ts_store_current(0,&winner); assert(copy == 1);
    nor[ts_address(0,1)+256u+20u] ^= 1u; project_load(0); assert(trk[0].step[0].note[0] == 60);
    /* A semantic error with a correct CRC still cannot replace the old copy. */
    assert(ts_store_current(0,&winner) == 0); ts_disk.bank[0] = 255;
    assert(!ts_store_write(0,0,winner.seq+1,sizeof ts_disk)); project_load(0); assert(trk[0].step[0].note[0] == 60);
    memcpy(nor,baseline,sizeof nor); assert(ts_store_current(0,&winner)==0);
    winner.seq=0xFFFFFFFFu;winner.hcrc=ts_crc(&winner,sizeof winner-4u);memcpy(nor+ts_address(0,0),&winner,sizeof winner);
    project_load(0);trk[0].step[0].note[0]=72;assert(!project_save(0));
    assert(ts_store_current(0,&winner)==1 && winner.seq==0);project_load(0);assert(trk[0].step[0].note[0]==72);
    for (uint32_t slot=0; slot<4; slot++) { trk[0].step[0].note[0] = (uint8_t)(48+slot); assert(!project_save(slot)); }
    for (uint32_t slot=0; slot<4; slot++) { project_load(slot); assert(trk[0].step[0].note[0] == 48+slot); }
    assert(!project_rename(2,"NAMED")); project_load(2); assert(!strcmp(proj_name,"NAMED") && trk[0].step[0].note[0] == 50);
    assert(!ts_project_save_as(4,"RESTORE")); memset(&ts_run,0,sizeof ts_run); assert(ts_project_load(4)); assert(ts_run.enabled);
    /* A later legacy-mode save must supersede an earlier tracker autosave. */
    memset(&ts_run,0,sizeof ts_run); assert(!ts_project_save_as(4,"LEGACY")); assert(ts_project_load(4)); assert(!ts_run.enabled);
    assert(!project_erase(1)); assert(ts_project_fetch(1) && !project_used(1));
    assert(ts_crc(nor,TS_STORE_BASE) == before_low && ts_crc(nor+0xC8000,sizeof nor-0xC8000) == before_high);
    printf("tracker store: %u power cuts, CRC/semantic fallback, four slots, rename, erase, restore-last and region canaries passed\n",cases);
}
