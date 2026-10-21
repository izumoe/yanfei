#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Compatibility entry point for the Yanfei profile.
set -eu
cd "$(dirname "$0")"
exec ./build-yanfei.sh "$@"
