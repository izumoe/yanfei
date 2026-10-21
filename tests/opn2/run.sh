#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# OPN-1's reference comparison, using the vendored/adapted core.
set -eu
cd "$(dirname "$0")/../.."
B=build/host/opn2
mkdir -p "$B"
${CC:-cc} -std=c99 -O2 -Wall -c firmware/src/opn2/opn2.c -o "$B/opn2.o"
${CC:-cc} -O2 -c tests/opn2/ref/nuked-opn2/ym3438.c -o "$B/ym3438.o"
for f in opn ssg adpcm; do
    ${CXX:-c++} -std=c++14 -O2 -c tests/opn2/ref/ymfm/ymfm_$f.cpp -o "$B/ymfm_$f.o"
done
${CXX:-c++} -std=c++14 -O2 -Wall -Itests/opn2/ref/ymfm -Ifirmware/src/opn2 -Itests/opn2/ref/nuked-opn2 \
    tests/opn2/opn2_compare.cpp "$B/opn2.o" "$B/ym3438.o" "$B/ymfm_opn.o" "$B/ymfm_ssg.o" "$B/ymfm_adpcm.o" -o "$B/compare"
"$B/compare" "$B"
${CC:-cc} -O2 -Wall -c tests/opn2/mono_probe.c -o "$B/mono.o"
${CC:-cc} -O2 -Wall tests/opn2/mono_compare.c "$B/mono.o" "$B/opn2.o" -o "$B/mono_compare"
"$B/mono_compare"
