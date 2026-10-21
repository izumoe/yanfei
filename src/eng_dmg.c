/* SPDX-License-Identifier: MIT */
/* Handheld-inspired: stepped 32 x 4-bit wave RAM, pulse, short/long noise, 64 Hz decay. */
#include "chip_common.h"
static const char *const DMG_VOICES[]={"PULSE","WAVE","NOISE"};
static const char *const DMG_NOISE[]={"15BIT","7BIT"};
static int32_t dmg_amp(track_t *t,voice_t *v,int32_t a) { return chip_amp_clock(t,v,a,64); }
static void dmg_render(track_t *t,voice_t *v,int32_t *o,uint32_t n,const vmod_t *m) { chip_render(t,v,o,n,m,1); }
static const char *const DMG_DUTIES[]={"12.5%","12.5%","12.5%","12.5%","25%","25%","25%","25%",
                                    "50%","50%","50%","50%","75%","75%","75%","75%"};
static const param_desc_t DMG_DUTY={"DUTY",F_ENUM,0,15,3,DMG_DUTIES,0};
static const param_desc_t *dmg_desc(const track_t *t,uint32_t k) { return k==1u && t->p[P_E0]==0?&DMG_DUTY:0; }
static const preset_t DMG_PRESETS[]={
    {"HANDHELD",{0,4,0,0,0,0,10,127},{0,60,90,30},0,1,FX(0,0,20,10),PAT(4)},
    {"WAVE LEAD",{1,11,0,0,0,0,18,110},{0,70,90,30},0,1,FX(0,0,40,25),PAT(3)},
    {"SHORT HIT",{2,0,0,1,0,2,0,127},{0,50,0,20},0,0,FX(0,0,0,0),PAT(12)},
    {"POCKET BASS",{0,8,0,0,0,0,0,94},{0,52,104,22},0,1,FX(0,0,0,0),PAT(2)},
    {"POCKET BELL",{1,9,18,0,0,0,0,116},{0,68,0,66},0,0,FX(0,6,24,34),PAT(7)},
    {"POCKET WIND",{2,0,0,0,0,0,0,58},{68,88,78,74},0,0,FX(0,16,18,44),PAT(5)},
    {"7BIT TOM",{2,0,0,1,62,4,0,88},{0,32,0,18},0,0,FX(0,0,0,8),PAT(12)},
    {"LINK LEAD",{0,4,0,0,0,0,24,123},{0,64,106,30},0,1,FX(0,6,26,18),PAT(4)},
};
static const engine_t ENG_DMG={
    .name="DMG",.page_title={"WAVE","MOTN"},
    .edit={{"VOICE",F_ENUM,0,2,1,DMG_VOICES,0},{"WAV#",F_ENUM,0,15,3,N_WRAM,0},
           {"MOTION",F_PCT,0,127,0,0,0},{"NOISE",F_ENUM,0,1,0,DMG_NOISE,0},
           {"SWP",F_PCT,0,127,0,0,0},{"DCY",F_ENUM,0,31,0,CHIP_DECAY,0},
           {"VIB",F_PCT,0,127,0,0,0},{"TONE",F_PCT,0,127,127,0,0}},
    .presets=DMG_PRESETS,.npresets=NELEM(DMG_PRESETS),.knob={P_E0,P_E1,P_E2,P_REL},
    .keep=1,.note_on=chip_note_on,.render=dmg_render,.amp=dmg_amp,.desc=dmg_desc,
};
