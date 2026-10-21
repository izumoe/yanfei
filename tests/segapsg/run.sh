#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build
${CC:-cc} -std=c99 -O2 -Wall -c src/segapsg/psg.c -o build/segapsg.o
${CXX:-c++} -std=c++14 -O2 -Wall -Isrc/segapsg tests/segapsg/psg_compare.cpp build/segapsg.o -o build/segapsg_compare
build/segapsg_compare
