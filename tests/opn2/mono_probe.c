/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile the instrument's one-channel variant beside the default six-channel core. */
#define OPN2_CHANNELS 1u
#define opn2_reset mono_reset
#define opn2_write mono_write
#define opn2_write_reg mono_write_reg
#define opn2_render mono_render
#define opn2_clock mono_clock
#include "../../firmware/src/opn2/opn2.c"
static opn2_t mono;
void mono_init(void){mono_reset(&mono);}
void mono_reg(unsigned r,unsigned v){mono_write_reg(&mono,0,(uint8_t)r,(uint8_t)v);}
void mono_samples(int32_t *out,unsigned n){mono_render(&mono,out,n);}
void mono_channel(void *out){
    uint8_t *d=out,*s=(uint8_t *)&mono.ch[0];
    for(unsigned i=0;i<sizeof mono.ch[0];i++)d[i]=s[i];
}
