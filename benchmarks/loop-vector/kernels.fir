// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

module @loop_bench {
func @forge_loop_add_vector(%base: ptr, %count: i64, %delta: i64) -> void {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%base, %count)
loop(%p: ptr, %n: i64):
  %done = cmp.eq i64 %n %zero
  branch %done, exit(), body(%p, %n)
body(%q: ptr, %m: i64):
  %v = load i64 %q align 8
  %nv = add i64 %v %delta
  store i64 %nv %q align 8
  %nextp = ptr.offset ptr %q 8
  %nextn = sub i64 %m %one
  jump loop(%nextp, %nextn)
exit:
  return
}
func @forge_loop_add_scalar(%base: ptr, %count: i64, %delta: i64) -> void {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%base, %zero)
loop(%p: ptr, %i: i64):
  %done = cmp.ge i64 %i %count
  branch %done, exit(), body(%p, %i)
body(%q: ptr, %j: i64):
  %v = load i64 %q align 8
  %nv = add i64 %v %delta
  store i64 %nv %q align 8
  %nextp = ptr.offset ptr %q 8
  %nexti = add i64 %j %one
  jump loop(%nextp, %nexti)
exit:
  return
}
}
