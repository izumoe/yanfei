#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host build/gen firmware/engines/build
python3 tests/wave_bank_test.py
python3 tools/gen_fm6_patches.py build/gen/felucca_fm6.h
${CC:-cc} -fwrapv -O1 -g -w -Ibuild/gen -Ifirmware/src -o build/host/core_test tests/core_test.c -lm
build/host/core_test
${CC:-cc} -fwrapv -O2 -ffp-contract=off -g -w -Ibuild/gen -Ifirmware/src \
    -o build/host/core_tracker_test tests/core_tracker_test.c -lm
build/host/core_tracker_test
for tracker_case in library overview section store backup audio; do
    ${CC:-cc} -fwrapv -O2 -ffp-contract=off -g -w -Ibuild/gen -Ifirmware/src \
        "tests/core_tracker_${tracker_case}_test.c" -lm -o "build/host/core_tracker_${tracker_case}_test"
    "build/host/core_tracker_${tracker_case}_test"
done
python3 firmware/engines/tools/gen_engine_tables.py firmware/engines/build/fe_tables.h
${CC:-cc} -O2 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Ifirmware/engines/build \
    -o build/host/chip_test firmware/engines/tests/chip_test.c -lm
build/host/chip_test

${CC:-cc} -fwrapv -O1 -g -w -Ibuild/gen -Ifirmware/src -o build/host/core_editor_test tests/core_editor_test.c -lm
build/host/core_editor_test
${CC:-cc} -DFELUCCA_CORE=1 -fwrapv -O1 -g -w -Ibuild/gen -Ifirmware/src -o build/host/core_fm6bank_test tests/fm6bank_test.c -lm
build/host/core_fm6bank_test

${CC:-cc} -DFELUCCA_CORE=1 -fwrapv -O2 -w -Ibuild/gen -Ifirmware/src -pthread \
    -o build/host/core_dualcore_test tests/dualcore_test.c -lm
build/host/core_dualcore_test

${CC:-cc} -fwrapv -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src \
    -o build/host/core_mix_probe tests/core_mix_probe.c -lm
python3 tests/core_mix_health.py

${CC:-cc} -fwrapv -O1 -g -w -Ibuild/gen -Ifirmware/src -o build/host/core_opn_test tests/core_opn_test.c -lm
build/host/core_opn_test
sh firmware/engines/tests/segapsg/run.sh
