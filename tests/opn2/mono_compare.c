/* SPDX-License-Identifier: GPL-3.0-only */
#include "../../firmware/src/opn2/opn2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void mono_init(void);
void mono_reg(unsigned r,unsigned v);
void mono_samples(int32_t *out,unsigned n);
void mono_channel(void *out);
static uint32_t rng=1;
static uint32_t rnd(void){rng=rng*1664525u+1013904223u;return rng>>8;}
int main(void){
 opn2_t six;opn2_reset(&six);mono_init();
 const unsigned banks[]={0x30,0x40,0x50,0x60,0x70,0x80,0x90};
 unsigned frames=0;
 for(unsigned k=0;k<12000;k++){
  unsigned r,v=rnd()&255u;
  if(k%7==0)r=0xb0; /* every algorithm and feedback value */
  else if(k%7==1)r=0x22;
  else if(k%7==2)r=0xb4;
  else if(k%7==3){r=0x28;v&=0xf0u;}
  else if(k%7==4){r=0xa4;v&=63u;}
  else if(k%7==5)r=0xa0;
  else r=banks[rnd()%7u]+4u*(rnd()%4u);
  opn2_write_reg(&six,0,(uint8_t)r,(uint8_t)v);mono_reg(r,v);
  int32_t a[128],b[128];unsigned n=1u+rnd()%64u;
  opn2_render(&six,a,n);mono_samples(b,n);frames+=n;
  for(unsigned i=0;i<n*2u;i++){
   int32_t d=a[i]-b[i];
   /* Five idle channels add ladder(0)=4 each before integer scaling. */
   assert(d>=419 && d<=421);
  }
  opn2_ch_t ch;mono_channel(&ch);
  assert(!memcmp(&ch,&six.ch[0],sizeof ch));
 }
 printf("OPN mono: %u random-write samples match six-channel operator state and output (idle DAC offset removed)\n",frames);
}
