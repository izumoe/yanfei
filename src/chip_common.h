/* SPDX-License-Identifier: MIT
 * Shared fixed-point chip primitives. No allocation or additional voice storage. */
#ifndef FE_CHIP_COMMON
#define FE_CHIP_COMMON
#include "chip_tables.h"
static const char *const CHIP_ARP[] = {"OFF", "OCT", "MAJ", "MIN"};
static const int8_t CHIP_ARPS[4][3] = {{0,0,0},{0,12,0},{0,4,7},{0,3,7}};
static const char *const CHIP_DECAY[] = {
    "CONST","D0","D1","D2","D3","D4","D5","D6","D7","D8","D9","D10","D11","D12","D13","D14","D15",
    "L1","L2","L3","L4","L5","L6","L7","L8","L9","L10","L11","L12","L13","L14","L15"};
static void chip_note_on(track_t *t, voice_t *v)
{
    uint32_t i;
    for (i=0;i<8;i++) v->s[i]=0;
    v->s[2]=0x7fff;
    v->s[4]=15;
    uint32_t mode=(uint32_t)t->p[P_E5]&31u;
    v->s[6]=(int32_t)(mode>16u?mode-16u:mode?mode-1u:0u);
}
/* Decay in 16 volume steps; note-off uses the host release, never a stuck looping envelope. */
static int32_t chip_amp_clock(track_t *t, voice_t *v, int32_t adsr, uint32_t hz)
{
    uint32_t mode=(uint32_t)t->p[P_E5]&31u, period=mode>16u?mode-16u:mode?mode-1u:0u;
    if (!v->active) return 0;
    if (mode) {
        v->s[1]+=(int32_t)(hz*CTL*65536u/FS);
        while (v->s[1]>=65536) {
            v->s[1]-=65536;
            if (v->s[6]) v->s[6]--;
            else {
                v->s[6]=(int32_t)period;
                if (v->s[4]) v->s[4]--;
                else if (mode>16u) v->s[4]=15;
            }
        }
    }
    return mulq15(adsr, (mode?v->s[4]:15)*Q4);
}
static uint32_t chip_noise_step(uint32_t l, uint32_t short_mode, uint32_t dmg)
{
    uint32_t bit=(l^(l>>(short_mode&&!dmg?6:1)))&1u;
    l=(l>>1)|(bit<<14);
    if (short_mode&&dmg) l=(l&~64u)|(bit<<6); /* handheld's seven-bit feedback */
    return l;
}
static int32_t chip_triangle(uint32_t ph)
{
    uint32_t k=ph>>27;
    return (int32_t)(k<16u?15u-k:k-16u)*(2*Q4)-15*Q4;
}
/* Reapply the host's sub-semitone tune/unison/bend residual after pitch changes. */
static uint32_t chip_pitch_inc(int32_t pitch, const vmod_t *m)
{
    uint32_t inc=pitch_inc(clamp(pitch,0,2047));
    return inc+(uint32_t)((int32_t)(inc>>12)*m->fine);
}
/* dmg=0: NES pulse/triangle/noise; dmg=1: handheld pulse/wave RAM/noise. */
static void chip_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m, uint32_t dmg)
{
    const int16_t *p=t->p;
    static const uint8_t duties[4]={1,2,4,6};
    uint32_t type=(uint32_t)p[P_E0]%3u, k, ph=v->ph[0], np=v->ph[1], aux=v->ph[2], l=(uint32_t)v->s[2];
    uint32_t tick=(uint32_t)v->s[5], wav=(uint32_t)p[P_E1]&15u;
    int32_t pitch=m->pitch16, lp=v->s[0], dc=0;
    uint32_t duty=0x20000000u*duties[(uint32_t)p[P_E1]&3u];
    if(dmg) duty=0x20000000u*duties[((uint32_t)p[P_E1]>>2)&3u];
    int32_t main_gain=32767,sub_gain=0;
    if(!dmg && p[P_E2]) {
        main_gain=32767*127/(127+p[P_E2]);
        sub_gain=32767*p[P_E2]/(127+p[P_E2]);
    }
    if (v->s[3]<4096) v->s[3]+=p[P_E4]/8;
    pitch-=v->s[3];
    if (!dmg) pitch+=CHIP_ARPS[(uint32_t)p[P_E6]&3u][(tick/28u)%3u]*16;
    else if (p[P_E6]) pitch+=(osc_sine(aux)*p[P_E6])/(32768*4);
    uint32_t inc=chip_pitch_inc(pitch,m);
    uint32_t ni=inc>0x0c000000u?0x60000000u:inc*8u;
    if (dmg && type==1u) {
        if (p[P_E2]) wav=(wav+tick/(4u+(127u-(uint32_t)p[P_E2])*2u))&15u;
        for (k=0;k<16u;k++) dc+=(WRAM[wav][k]>>4)+(WRAM[wav][k]&15)-15;
        dc=dc*(2*Q4)/32;
    }
    int32_t tone=clamp(3000+p[P_E7]*256+m->cutoff,3000,32767);
    for (k=0;k<n;k++) {
        uint32_t next=np+ni;
        if (next<np) l=chip_noise_step(l,(uint32_t)p[P_E3]&1u,dmg);
        np=next;
        int32_t s;
        if (type==2u) s=(l&1u)?32760:-32760;
        else if (type==1u && !dmg) s=chip_triangle(ph);
        else if (type==1u) {
            uint32_t j=ph>>27,b=WRAM[wav][j>>1];
            s=((int32_t)(j&1u?b&15u:b>>4)*2-15)*Q4-dc;
        } else s=(ph<duty?32760:-32760)-((int32_t)(duty>>16)-32768);
        if (!dmg && p[P_E2]) s=mulq15(s,main_gain)+mulq15(chip_triangle(aux),sub_gain);
        lp+=mulq15(s-lp,tone);
        out[k]+=voice_amp(lp,m,k);
        ph+=inc;
        if (!dmg) aux+=inc>>1;
    }
    if (dmg) aux+=0x01000000u; /* control-clock vibrato */
    v->ph[0]=ph;v->ph[1]=np;v->ph[2]=aux;
    v->s[0]=lp;v->s[2]=(int32_t)l;v->s[5]=(int32_t)((tick+1u)&0x3fffffffu);
}
#endif
