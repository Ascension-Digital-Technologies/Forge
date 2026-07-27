# Copyright 2026 Mario Vinciguerra
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    cmake --preset release-strict
    cmake --build --preset release-strict
    ctest --preset release-strict
    Write-Host "FORGE  release gate passed"
} finally {
    Pop-Location
}
