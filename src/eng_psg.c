/* SPDX-License-Identifier: MIT */
/* AY-inspired tone/noise mixer and repeating 16-step buzzer envelopes.
 * Rate spans slow modulation to pitched audio-rate envelopes; not register emulation. */
#include "chip_common.h"
#include "segapsg/psg.c"
/* Sega chip state is per voice; AY retains its compact voice_t state. */
static segapsg_t *psg_sega_of(track_t *t,voice_t *v)
{
    return (segapsg_t *)ENG_MEM((uint32_t)(t-trk))+(uint32_t)(v-t->v);
}
ENG_MEM_FITS("PSG",sizeof(segapsg_t)*NVOICE);
static const char *const PSG_CHIPS[]={"AY","SEGA"};
static const char *const PSG_SHAPES[]={"FLAT","DOWN","UP","TRI","RTRI","D-HLD","U-HLD","PULSE"};
static inline uint32_t psg_env_level(uint32_t shape,uint32_t pos,uint32_t held)
{
    switch(shape) {
    case 1:return 15-pos; case 2:return pos;
    case 3:return pos<8?15-pos*2:pos*2-16;
    case 4:return pos<8?pos*2:31-pos*2;
    case 5:return held?0:15-pos; case 6:return held?15:pos;
    case 7:return pos<8?15:0; default:return 15;
    }
}
static void psg_note_on(track_t *t,voice_t *v)
{
    chip_note_on(t,v);
    v->s[4]=0; /* envelope phase retriggers independently of free-running tone phases */
    v->s[6]=t->p[P_ED_FX]==1;
    if(v->s[6])segapsg_reset(psg_sega_of(t,v));
}
static void psg_render_ay(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    const int16_t *p=t->p;
    uint32_t ph=v->ph[0],ph2=v->ph[1],ep=(uint32_t)v->s[4],i,l=(uint32_t)v->s[2],np=(uint32_t)v->s[7];
    uint32_t tick=(uint32_t)v->s[5],shape=(uint32_t)p[P_E4]&7u;
    uint32_t held=(uint32_t)v->s[3];
    int32_t pitch=m->pitch16+CHIP_ARPS[(uint32_t)p[P_E6]&3u][(tick/28u)%3u]*16;
    uint32_t inc=chip_pitch_inc(pitch,m),inc2=chip_pitch_inc(pitch+p[P_E1]*16,m);
    uint32_t noise_inc=0x60000000u/(1u+(uint32_t)p[P_E2]);
    uint32_t env_inc=pitch_inc((uint32_t)p[P_E5]*16u)>>2;
    int32_t tone_gain=32767*127/(127+p[P_E0]),tone2_gain=32767*p[P_E0]/(127+p[P_E0]);
    int32_t lp=v->s[0],tone=clamp(3000+p[P_E7]*256+m->cutoff,3000,32767);
    for(i=0;i<n;i++) {
        uint32_t next=np+noise_inc;
        if(next<np) l=chip_noise_step(l,0,0);
        np=next;
        int32_t a=ph<0x80000000u?32760:-32760,b=ph2<0x80000000u?32760:-32760;
        int32_t s=mulq15(a,tone_gain)+mulq15(b,tone2_gain);
        s=mulq15(s,(127-p[P_E3])*258)+mulq15((l&1u)?32760:-32760,p[P_E3]*258);
        uint32_t vol=psg_env_level(shape,ep>>28,held);
        s=mulq15(s,(int32_t)vol*Q4);
        lp+=mulq15(s-lp,tone);
        out[i]+=voice_amp(lp,m,i);
        ph+=inc;ph2+=inc2;
        next=ep+env_inc;if(next<ep)held=1;ep=next;
    }
    v->ph[0]=ph;v->ph[1]=ph2;v->s[4]=(int32_t)ep;v->s[0]=lp;v->s[2]=(int32_t)l;v->s[3]=(int32_t)held;
    v->s[7]=(int32_t)np;v->s[5]=(int32_t)((tick+1u)&0x3fffffffu);
}
/* Sega PSG: 10-bit tone periods, 16-bit noise, 2 dB level steps, no AY buzzer.
 * Three fixed noise clocks plus a tone-linked clock. N-DIV affects only LINK. */
static const char *const PSG_SEGA_NOISE[]={"W-HI","W-MID","W-LO","W-LNK","P-HI","P-MID","P-LO","P-LNK"};
static const param_desc_t PSG_SEGA_DESC[3]={
    {"N-DIV",F_INT,0,31,8,0,0},{"NOISE",F_ENUM,0,7,0,PSG_SEGA_NOISE,0},{"VOL",F_INT,0,127,127,0,0}
};
static const param_desc_t *psg_desc(const track_t *t,uint32_t k)
{
    if(t->p[P_ED_FX]!=1)return 0;
    return k==2?&PSG_SEGA_DESC[0]:k==4?&PSG_SEGA_DESC[1]:k==5?&PSG_SEGA_DESC[2]:0;
}
/* AY's signature controls are its buzzer envelope; Sega has no buzzer and
 * instead puts noise mode and brightness on HOME. Volume stays in EDIT. */
static uint8_t psg_home_knob(const track_t *t,uint32_t k)
{
    static const uint8_t ay[4]={P_E0,P_E3,P_E4,P_E5};
    static const uint8_t sega[4]={P_E0,P_E3,P_E4,P_E7};
    return (t->p[P_ED_FX]==1?sega:ay)[k&3u];
}
/* Nearest real 10-bit period, at the console's master clock / 32. */
static uint32_t psg_sega_period(uint32_t inc)
{
    const uint64_t clock=(3579545ull<<32)/(32u*FS);
    uint32_t div=inc?(uint32_t)((clock+inc/2u)/inc):1023u;
    if(div<1)div=1;
    if(div>1023)div=1023;
    return div;
}
static void psg_sega_tone(segapsg_t *s,uint32_t ch,uint32_t period)
{
    if((uint32_t)s->reg[ch*2u]==period)return;
    segapsg_write(s,(uint8_t)(0x80u|(ch<<5)|(period&15u)));
    segapsg_write(s,(uint8_t)(period>>4));
}
static void psg_sega_volume(segapsg_t *s,uint32_t ch,uint32_t target)
{
    /* MAME's exact 2 dB ladder, including the hard mute. TONE2/MIX are
     * instrument controls translated to channel attenuation registers. */
    uint32_t best=15,err=target;
    for(uint32_t i=0;i<15;i++) {
        uint32_t a=(uint32_t)SEGAPSG_VOL[i],d=a>target?a-target:target-a;
        if(d<err){best=i;err=d;}
    }
    if((uint32_t)s->reg[ch*2u+1u]!=best)segapsg_write(s,(uint8_t)(0x90u|(ch<<5)|best));
}
static void psg_render_sega(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    const int16_t *p=t->p;segapsg_t *s=psg_sega_of(t,v);
    uint32_t tick=(uint32_t)v->s[5],mode=(uint32_t)p[P_E4]&7u;
    int32_t pitch=m->pitch16+CHIP_ARPS[(uint32_t)p[P_E6]&3u][(tick/28u)%3u]*16;
    uint32_t d0=psg_sega_period(chip_pitch_inc(pitch,m));
    uint32_t d1=psg_sega_period(chip_pitch_inc(pitch+p[P_E1]*16,m));
    uint32_t dn=d1*(1u+(uint32_t)p[P_E2]);if(dn>1023u)dn=1023u;
    psg_sega_tone(s,0,d0);psg_sega_tone(s,1,d1);psg_sega_tone(s,2,dn);
    uint32_t nr=(mode&3u)|(mode<4u?4u:0u);
    if((uint32_t)s->reg[6]!=nr)segapsg_write(s,(uint8_t)(0xe0u|nr));
    uint32_t lev=(uint32_t)p[P_E5]>>3,base=(uint32_t)SEGAPSG_VOL[15u-lev];
    uint32_t tones=base*(127u-(uint32_t)p[P_E3])/127u;
    psg_sega_volume(s,0,tones*127u/(127u+(uint32_t)p[P_E0]));
    psg_sega_volume(s,1,tones*(uint32_t)p[P_E0]/(127u+(uint32_t)p[P_E0]));
    psg_sega_volume(s,2,0);psg_sega_volume(s,3,base*(uint32_t)p[P_E3]/127u);
    int32_t native[CTL*2]={0};
    segapsg_render(s,native,n,(uint32_t)(((uint64_t)SEGAPSG_CLOCK<<16)/(16u*FS)));
    int32_t lp=v->s[0],tone=clamp(3000+p[P_E7]*256+m->cutoff,3000,32767);
    int32_t total=s->volume[0]+s->volume[1]+s->volume[3];
    for(uint32_t i=0;i<n;i++) {
        int32_t x=(native[i*2u]*2+total)*4;
        lp+=mulq15(x-lp,tone);out[i]+=voice_amp(lp,m,i);
    }
    v->s[0]=lp;v->s[5]=(int32_t)((tick+1u)&0x3fffffffu);
}
static void psg_render(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    int sega=t->p[P_ED_FX]==1;
    if(v->s[6]!=sega){
        if(sega)segapsg_reset(psg_sega_of(t,v));
        v->s[2]=0x7fff;v->s[7]=0;v->s[6]=sega;v->s[4]=v->s[3]=0;
    }
    if(sega)psg_render_sega(t,v,out,n,m);else psg_render_ay(t,v,out,n,m);
}
static const preset_t PSG_PRESETS[]={
    {"BUZZ BASS",{0,0,8,0,1,48,0,127},{0,70,100,35},0,1,FX(0,0,10,0),PAT(2)},
    {"PSG CHORD",{96,7,12,0,0,40,0,100},{0,70,100,35},0,0,FX(0,0,30,20),PAT(6)},
    {"SEGA HIT",{0,0,16,127,0,127,0,100},{0,50,0,25},0,0,FX(0,0,0,10),PAT(12)},
    {"AY BUZZ LEAD",{18,12,8,0,3,72,0,120},{0,64,100,28},0,1,FX(0,0,24,14),PAT(4)},
    {"AY ZAP",{0,0,5,58,2,96,0,118},{0,35,0,22},0,0,FX(0,0,12,14),PAT(12)},
    {"SEGA SQUARE",{0,0,0,0,0,127,0,127},{0,65,104,25},0,1,FX(0,0,22,10),PAT(4)},
    {"SEGA CHORD",{102,7,0,0,0,120,0,112},{2,64,94,38},0,0,FX(0,8,24,22),PAT(6)},
    {"SEGA HAT",{0,0,0,127,0,119,0,127},{0,24,0,16},0,0,FX(0,0,0,8),PAT(12)},
};
static const engine_t ENG_PSG={
    .name="PSG",.page_title={"MIX","COLOR"},
    .edit={{"TONE2",F_PCT,0,127,0,0,0},{"INT",F_SEMI,-24,24,0,0,0},
           {"N-DIV",F_INT,0,31,8,0,0},{"N-MIX",F_PCT,0,127,0,0,0},
           {"BUZZ",F_ENUM,0,7,0,PSG_SHAPES,0},{"B-RATE",F_INT,0,127,48,0,0},
           {"ARP",F_ENUM,0,3,0,CHIP_ARP,0},{"TONE",F_PCT,0,127,127,0,0}},
    .presets=PSG_PRESETS,.npresets=NELEM(PSG_PRESETS),.knob={P_E0,P_E3,P_E4,P_E5},
    .keep=1,.note_on=psg_note_on,.render=psg_render,.desc=psg_desc,
    .mem=sizeof(segapsg_t)*NVOICE,
    .home_knob=psg_home_knob,
};
