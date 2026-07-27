#!/usr/bin/env sh
# Copyright 2026 Mario Vinciguerra
# SPDX-License-Identifier: Apache-2.0

set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
cmake --preset release-strict
cmake --build --preset release-strict
ctest --preset release-strict
printf '%s\n' 'FORGE  release gate passed'
