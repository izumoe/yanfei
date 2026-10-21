/* SPDX-License-Identifier: GPL-3.0-only */
/* YM instrument: OPN-1's integer ymfm port (BSD-3-Clause, opn2/opn2.c).
 * Each voice uses one chip channel at the NTSC native rate, linearly resampled
 * to FS. Eight algorithms, real OPN attenuation/rates, detune, SSG, PM/AM and
 * ladder DAC behavior. D2R, SSG and KS are shared controls. No VGM/PCM streaming,
 * timers/CSM or channel-3 special mode exposed by this instrument.
 * Per-voice scratch lives in the existing workspace, safe across both CPUs. */
#define OPN2_CHANNELS 1u
#include "opn2/opn2.c"
#define YM_POLY 6u
#define YM_NATIVE_MAX (CTL*2u)
typedef struct {
    opn2_t chip;
    int32_t native[YM_NATIVE_MAX*2u];
    int32_t prev,last,dc;
    uint32_t frac;
    uint8_t gate,live;
} ym_voice_t;
ENG_MEM_FITS("YM",sizeof(ym_voice_t)*YM_POLY);
static const uint8_t YM_CARRIERS[8]={8,8,8,8,10,14,14,15};
static const char *const YM_SSG_NAMES[]={"OFF","DOWN","D-HOLD","D-ALT","D-AHLD","UP","U-HOLD","U-ALT","U-AHLD"};
static const char *const YM_LFO_NAMES[]={"OFF","3.98","5.56","6.02","6.37","6.88","9.63","48.1","72.2"};
static ym_voice_t *ym_voice_of(track_t *t,voice_t *v)
{
    uint32_t tr=(uint32_t)(t-trk),vi=(uint32_t)(v-t->v);
    return tr<NPART && vi<YM_POLY?(ym_voice_t *)ENG_MEM(tr)+vi:0;
}
/* Avoid redundant writes: a change prepares operator caches once a block. */
static void ym_reg(opn2_t *c,uint32_t r,uint32_t data)
{
    if(c->regs[r]!=(uint8_t)data)opn2_write_reg(c,0,(uint8_t)r,(uint8_t)data);
}
static void ym_note_on(track_t *t,voice_t *v)
{
    ym_voice_t *n=ym_voice_of(t,v);if(!n)return;
    memset(n,0,sizeof *n);opn2_reset(&n->chip);
    opn2_write_reg(&n->chip,0,0x28,0xf0);
    n->gate=n->live=1;
}
static int ym_done(track_t *t,voice_t *v)
{
    ym_voice_t *n=ym_voice_of(t,v);if(!n || !n->live)return 1;
    if(!v->gate && n->gate){opn2_write_reg(&n->chip,0,0x28,0);n->gate=0;return 0;}
    /* A silent attack (AR 0) must stay alive until released. The chip's own
     * attenuation, including SSG inversion, determines when carriers finish. */
    uint32_t carriers=YM_CARRIERS[(uint32_t)t->p[P_E0]&7u];
    for(uint32_t op=0;op<4;op++)if(carriers>>op&1u) {
        const opn2_op_t *o=&n->chip.ch[0].op[op];
        if(o->keyon_live || o->state!=opn2_EG_RELEASE || o->env<0x3ffu)return 0;
    }
    n->live=0;return 1;
}
static void ym_render(track_t *t,voice_t *v,int32_t *out,uint32_t count,const vmod_t *m)
{
    ym_voice_t *n=ym_voice_of(t,v);if(!n || !n->live || count>CTL)return;
    opn2_t *c=&n->chip;
    static const uint8_t slot[4]={0,8,4,12};
    uint32_t fb=(uint32_t)clamp(t->p[P_E1]+((m->shape-(64<<8))>>12),0,7);
    ym_reg(c,0xb0,((uint32_t)t->p[P_E0]&7u)|(fb<<3));
    uint32_t lfo=(uint32_t)t->p[P_E4];
    ym_reg(c,0x22,lfo?8u|((lfo-1u)&7u):0u);
    ym_reg(c,0xb4,0xc0u|((uint32_t)t->p[P_E6]<<4)|(uint32_t)t->p[P_E5]);
    for(uint32_t op=0;op<4;op++) {
        uint32_t id=P_FM1_ATK+op*5u,o=slot[op];
        int32_t dt=t->p[P_LN4+op];
        ym_reg(c,0x30+o,((uint32_t)(dt<0?4-dt:dt)<<4)|(uint32_t)t->p[P_LN0+op]);
        ym_reg(c,0x40+o,(uint32_t)clamp(t->p[id+4]-(m->cutoff>>9),0,127));
        ym_reg(c,0x50+o,((uint32_t)t->p[P_FTYPE]<<6)|(uint32_t)t->p[id]);
        ym_reg(c,0x60+o,(((uint32_t)t->p[P_ED_FX]>>op&1u)<<7)|(uint32_t)t->p[id+1]);
        ym_reg(c,0x70+o,(uint32_t)t->p[P_E2]);
        ym_reg(c,0x80+o,((uint32_t)t->p[id+2]<<4)|(uint32_t)t->p[id+3]);
        uint32_t ssg=(uint32_t)t->p[P_E3];ym_reg(c,0x90+o,ssg?8u|((ssg-1u)&7u):0u);
    }
    /* inc = f * 2^32 / FS; native phase = FNUM * 2^BLOCK / 2,
     * with 20 phase bits. Round to the nearest representable chip pitch. */
    const uint64_t den=(uint64_t)OPN2_RATE_NTSC<<12;
    uint32_t fn=(uint32_t)(((uint64_t)m->inc*FS*2u+den/2u)/den),block=0;
    while(fn>2047u && block<7u){fn=(fn+1u)>>1;block++;}
    if(fn>2047u)fn=2047u;
    uint32_t hi=(block<<3)|(fn>>8),lo=fn&255u;
    if(c->regs[0xa4]!=hi || c->regs[0xa0]!=lo) {
        opn2_write_reg(c,0,0xa4,(uint8_t)hi);opn2_write_reg(c,0,0xa0,(uint8_t)lo);
    }
    uint32_t need=(n->frac+count*OPN2_RATE_NTSC)/FS;
    opn2_render(c,n->native,need);
    uint32_t at=0;
    int32_t vel=(v->mvel?v->mvel:v->vel)*258;
    int32_t gain=mulq15(vel,t->p[P_E7]*258);
    for(uint32_t i=0;i<count;i++) {
        n->frac+=OPN2_RATE_NTSC;
        while(n->frac>=FS){n->frac-=FS;n->prev=n->last;n->last=n->native[2u*at++];}
        int32_t sample=(n->prev+(int32_t)((int64_t)(n->last-n->prev)*n->frac/FS))*4;
        int32_t dry=sample-n->dc;n->dc+=dry>>9;
        out[i]+=voice_amp(mulq15(dry,gain),m,i);
    }
}
/* {AR,D1R,D1L,RR,TL,MUL,DT}. Operator numbering is OPN order. */
static const int8_t YM_PATCH[8][4][7]={
    {{31,20,5,12,28,1,0},{31,14,7,12,34,1,0},{31,18,6,12,24,2,0},{31,12,4,13,0,1,0}},
    {{31,18,6,12,30,1,0},{31,12,5,12,0,1,0},{31,20,7,12,36,2,1},{31,11,4,12,0,1,-1}},
    {{31,16,8,12,28,7,0},{31,12,9,12,30,3,0},{31,16,8,12,34,11,1},{31,10,7,12,0,1,0}},
    {{31,22,8,13,25,2,0},{31,14,3,13,0,1,0},{31,24,10,13,18,1,0},{31,16,4,13,10,1,0}},
    {{27,10,4,12,30,1,0},{29,12,3,12,4,1,0},{28,12,4,12,26,2,1},{28,11,3,12,14,1,-1}},
    {{31,20,5,13,31,1,0},{31,18,4,13,34,2,0},{31,16,3,13,24,1,0},{31,12,3,13,0,1,0}},
    {{31,22,10,12,36,14,0},{31,8,6,12,7,1,0},{31,18,8,12,48,1,0},{31,10,6,12,13,1,1}},
    {{31,18,9,12,30,3,0},{31,14,8,12,0,1,0},{31,20,10,12,32,7,0},{31,16,8,12,8,2,0}}

};
static void ym_preset_values(track_t *t,uint32_t pr)
{
    for(uint32_t op=0;op<4;op++) {
        const int8_t *p=YM_PATCH[pr%NELEM(YM_PATCH)][op];
        for(uint32_t j=0;j<5;j++)t->p[P_FM1_ATK+op*5u+j]=p[j];
        t->p[P_LN0+op]=p[5];t->p[P_LN4+op]=p[6];
    }
    t->p[P_FTYPE]=1;t->p[P_ED_FX]=15;
}
static const preset_t YM_PRESETS[]={
    {"MD BASS",{0,4,3,0,0,0,0,72},{0,0,127,0},0,1,FX(0,0,10,0),PAT(2)},
    {"FM BRASS",{4,3,1,0,3,3,1,48},{0,0,127,0},0,0,FX(0,10,20,20),PAT(4)},
    {"METAL BELL",{0,5,4,0,0,0,0,88},{0,0,127,0},0,0,FX(0,0,30,45),PAT(7)},
    {"MD SLAP",{4,5,2,0,0,0,0,86},{0,0,127,0},0,1,FX(0,0,8,8),PAT(2)},
    {"MD REED",{5,3,1,0,3,2,1,76},{0,0,127,0},0,1,FX(0,8,24,18),PAT(4)},
    {"ARCADE LEAD",{0,5,1,0,4,2,0,88},{0,0,127,0},0,1,FX(0,8,28,18),PAT(4)},
    {"MD EPIANO",{4,2,4,0,0,0,0,92},{0,0,127,0},0,0,FX(0,18,18,32),PAT(6)},
    {"MD CHIME",{4,3,5,0,0,0,0,96},{0,0,127,0},0,0,FX(0,8,26,42),PAT(7)}

};
_Static_assert(NELEM(YM_PATCH)==NELEM(YM_PRESETS),"YM operator data per preset");
static const engine_t ENG_YM={
    .name="YM",.page_title={"FM4","OPN"},
    .edit={{"ALG",F_INT,0,7,0,0,0},{"FB",F_INT,0,7,3,0,0},
           {"D2R",F_INT,0,31,2,0,0},{"SSG",F_ENUM,0,8,0,YM_SSG_NAMES,0},
           {"LFO",F_ENUM,0,8,0,YM_LFO_NAMES,"Hz"},{"PMS",F_INT,0,7,0,0,0},
           {"AMS",F_INT,0,3,0,0,0},{"LEVEL",F_PCT,0,127,100,0,0}},
    .presets=YM_PRESETS,.npresets=NELEM(YM_PRESETS),.knob={P_E0,P_E1,P_E2,P_E7},
    .poly=YM_POLY,.ownenv=1,.note_on=ym_note_on,.done=ym_done,.render=ym_render,
    .mem=sizeof(ym_voice_t)*YM_POLY
};
