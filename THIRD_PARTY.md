# Sega PSG core

`src/segapsg/psg.c` and `psg.h` come from putchdev/opn-1 at commit
3319da44abc76fd0ce208de7c37405feef5770fd. They are a BSD-3-Clause C port of
MAME's Sega PSG by Nicola Salmoria and contributors. Their full license notice
is retained in `psg.c`; reproduce it with binary distributions.

`tests/segapsg/psg_compare.cpp` is the OPN-1 GPL-3.0-only comparison test.
`tests/segapsg/mame_segapsg.h` retains MAME's BSD-3-Clause notice. These files
are test-only and are not compiled into the firmware or standalone host.
The MIT license on the remaining engine sources is unchanged.
