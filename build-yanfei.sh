#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Yanfei: twelve engines and the four-track tracker.
set -eu
cd "$(dirname "$0")"
export FELUCCA_CORE=1 FELUCCA_SLICE=0 FELUCCA_FM4=0
exec ./build.sh "$@"
