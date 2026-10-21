/* SPDX-License-Identifier: GPL-3.0-only */
/* No instrument PCM in the core profile. Shared transfer/decoder types still compile. */
static const uint8_t SMP_DATA[1] = {0};
static const smp_zone_t SMP_ZONES[1] = {{0}};
static const smp_set_t SMP_SETS[5] = {{"NONE",0,0},{"NONE",0,0},{"NONE",0,0},{"NONE",0,0},{"NONE",0,0}};
#define SMP_NSETS 5
#define SMP_NPRESETS 4
#define SMP_NALIAS 0
#define SMP_SET_NAMES_INIT "NONE", "NONE", "NONE", "NONE", "NONE"
static const char *const SMP_SET_NAMES[5] = {"NONE","NONE","NONE","NONE","NONE"};
static const uint8_t SMP_SET_ORIG[5] = {0,1,2,3,4};
static const preset_t SMP_PRESET_TABLE[5] = {{"NONE",{0},{0},0,0}};
