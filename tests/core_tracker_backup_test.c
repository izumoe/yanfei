/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_CORE 1
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
#include <assert.h>
static void put32(uint8_t *p,uint32_t v) { for(uint32_t i=0;i<5;i++) p[i]=(uint8_t)(v>>(7u*i))&127u; }
static uint32_t begin(uint8_t id,const uint8_t *raw,uint32_t len)
{
    uint8_t a[12]={0,id}; put32(a+2,len);put32(a+7,st_crc32(raw,len));return ed_bk_write(a,sizeof a);
}
static uint32_t chunk(uint8_t id,const uint8_t *raw,uint32_t off,uint32_t count)
{
    uint8_t a[310]={1,id};put32(a+2,off);uint32_t at=7;
    for(uint32_t i=0;i<count;) {uint32_t n=count-i>7?7:count-i, mask=at++;a[mask]=0;
        for(uint32_t k=0;k<n;k++,i++){a[mask]|=(raw[off+i]>>7)<<k;a[at++]=raw[off+i]&127u;}}
    return ed_bk_write(a,at);
}
static void restore(uint8_t id,const uint8_t *raw,uint32_t len)
{
    assert(!begin(id,raw,len));
    for(uint32_t off=0;off<len;off+=256) assert(!chunk(id,raw,off,len-off>256?256:len-off));
    uint8_t a[2]={2,id};assert(!ed_bk_write(a,2));
}
int main(void)
{
    reset(); assert(!ts_activate());
    trk[0].step[0]=(step_t){.note={60,64,67},.n=3,.time=ST_NOTE,.vel=101};
    assert(!ts_sync());ts_bank.phrase[0].gate[0]=48;ts_bank.order[3][0]=0;ts_bank.song_length=4;
    assert(!project_save_as(0,"BACKUP"));assert(!ed_bk_capture());
    uint32_t length;const uint8_t *data=ed_bk_object(0,&length);assert(data && length==sizeof ts_disk);
    static uint8_t saved[sizeof ts_disk];memcpy(saved,data,length);
    assert(ed_bk_object(2,&length) && length==sizeof ts_disk);
    data=ed_bk_object(0,&length);assert(data && !memcmp(data,saved,sizeof saved));
    ts_bank.phrase[0].gate[0]=12;assert(!ed_bk_object(0,&length));
    restore(0,saved,sizeof saved);assert(ts_run.enabled && ts_bank.phrase[0].gate[0]==48 && ts_bank.song_length==4);
    restore(3,saved,sizeof saved);project_load(1);assert(ts_run.enabled && trk[0].step[0].n==3 && ts_bank.phrase[0].gate[0]==48);
    assert(!ed_bk_capture());data=ed_bk_object(3,&length);assert(data && length==sizeof ts_disk && !memcmp(data,saved,length));
    /* Buffer reuse during a transfer is detected before any commit. */
    assert(!begin(4,saved,sizeof saved));assert(!chunk(4,saved,0,256));(void)ts_store_exists(0);
    assert(chunk(4,saved,256,256)==5 && !ed_bk_put);
    /* An actual y12 transfer keeps its old CRC length through the protocol. */
    static uint8_t old[15728];FILE *fixture=fopen("tests/fixtures/yanfei-y12-project.bin","rb");assert(fixture);
    assert(fread(old,1,sizeof old,fixture)==sizeof old && fgetc(fixture)==EOF);fclose(fixture);
    restore(0,old,sizeof old);assert(ts_run.enabled && !ts_section[0].beats && ts_bank.phrase[0].gate[0]==48);
    restore(4,old,sizeof old);project_load(2);assert(ts_run.enabled && !ts_section[0].beats && ts_bank.phrase[0].step[0].n==3);
    assert(!begin(4,old,sizeof old));assert(!chunk(4,old,0,256));(void)ts_store_exists(0);
    assert(chunk(4,old,256,256)==5 && !ed_bk_put);
    /* A valid old envelope in an incorrectly sized transfer is rejected. */
    static uint8_t wrong[sizeof ts_disk];memcpy(wrong,old,sizeof old);
    assert(!begin(0,wrong,sizeof wrong));
    for(uint32_t off=0;off<sizeof wrong;off+=256)assert(!chunk(0,wrong,off,sizeof wrong-off>256?256:sizeof wrong-off));
    uint8_t commit[2]={2,0};assert(ed_bk_write(commit,2)==2);
    restore(0,saved,sizeof saved);
    /* Legacy project imports replace a previously larger tracker object. */
    static project_store_t legacy;project_capture(&proj_scratch);assert(proj_pack(&legacy,&proj_scratch));
    restore(3,(const uint8_t *)&legacy,sizeof legacy);project_load(1);assert(!ts_run.enabled && trk[0].step[0].n==3);
    restore(3,0,0);assert(!project_used(1));
    restore(0,saved,sizeof saved);
    ts_bank.chain[0][0].phrase=1;assert(!ts_prepare(TS_CHAIN,0,0,0));seq_start();
    ed_shadow();assert(!seq_steps(TSEL)[63].n && !seq_steps(TSEL)[63].hit);
    int16_t length_before=TSEL->p[P_SLEN];uint8_t length_set[4]={0,P_SLEN,64,64};
    assert(request(ED_SET,length_set,4)>0 && TSEL->p[P_SLEN]==length_before);
    seq_stop();
    puts("tracker backup: complete working/stored libraries, chunked restore, stale buffers, legacy replacement and tombstones passed");
}
