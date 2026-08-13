// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
__attribute__((noinline)) void llvm_loop_add(int64_t *p, int64_t n, int64_t d) { for (int64_t i=0;i<n;i++) p[i]+=d; }
