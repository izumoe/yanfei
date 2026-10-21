/* SPDX-License-Identifier: MIT */
/* NES-inspired playable voice; no console channel-allocation or DPCM emulation. */
#include "chip_common.h"
static const char *const NES_VOICES[]={"PULSE","TRI","NOISE"};
static const char *const CHIP_DUTIES[]={"12.5%","25%","50%","75%"};
static const char *const CHIP_NOISE[]={"LONG","SHORT"};
static int32_t nes_amp(track_t *t,voice_t *v,int32_t a) { return chip_amp_clock(t,v,a,240); }
static void nes_render(track_t *t,voice_t *v,int32_t *o,uint32_t n,const vmod_t *m) { chip_render(t,v,o,n,m,0); }
/* LOFI PULSE LD and WAVE BASS adapted to the new controls, plus a noise hit. */
static const preset_t NES_PRESETS[]={
    {"PULSE LD",{0,1,0,0,0,0,0,127},{0,60,90,30},0,1,FX(0,0,40,20),PAT(4)},
    {"WAVE BASS",{1,0,0,0,0,0,0,90},{0,50,70,20},0,1,FX(0,0,10,0),PAT(2)},
    {"NOISE HIT",{2,0,0,1,60,3,0,100},{0,60,0,30},0,0,FX(0,0,0,10),PAT(12)},
    {"FAMI HERO",{0,0,0,0,0,0,0,127},{0,52,102,26},0,1,FX(0,0,24,12),PAT(4)},
    {"FAMI LOW",{1,0,0,0,0,0,0,76},{0,56,116,20},0,1,FX(0,0,0,0),PAT(2)},
    {"FAMI ARP",{0,2,14,0,0,0,2,119},{0,58,92,24},0,0,FX(0,0,24,10),PAT(13)},
    {"8BIT SNARE",{2,0,0,0,45,2,0,112},{0,36,0,18},0,0,FX(0,0,0,8),PAT(12)},
    {"DUNGEON PAD",{0,1,28,0,0,0,0,80},{52,76,108,68},0,0,FX(0,22,24,38),PAT(5)},
};
static const engine_t ENG_NES={
    .name="NES",.page_title={"CHIP","MOTN"},
    .edit={{"VOICE",F_ENUM,0,2,0,NES_VOICES,0},{"DUTY",F_ENUM,0,3,1,CHIP_DUTIES,0},
           {"SUB",F_PCT,0,127,0,0,0},{"NOISE",F_ENUM,0,1,0,CHIP_NOISE,0},
           {"SWP",F_PCT,0,127,0,0,0},{"DCY",F_ENUM,0,31,0,CHIP_DECAY,0},
           {"ARP",F_ENUM,0,3,0,CHIP_ARP,0},{"TONE",F_PCT,0,127,127,0,0}},
    .presets=NES_PRESETS,.npresets=NELEM(NES_PRESETS),.knob={P_E0,P_E1,P_E5,P_REL},
    .keep=1,.note_on=chip_note_on,.render=nes_render,.amp=nes_amp,
};
