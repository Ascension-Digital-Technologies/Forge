// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

module @broad_bench {
func @forge_int_mix(%seed: i64, %rounds: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %seed)
loop(%i: i64, %x: i64):
  %done = cmp.ge i64 %i %rounds
  branch %done, exit(%x), body(%i, %x)
body(%j: i64, %v: i64):
  %c1 = const i64 6364136223846793005
  %c2 = const i64 1442695040888963407
  %m = mul i64 %v %c1
  %a = add i64 %m %c2
  %thirteen = const i64 13
  %s1 = shr.unsigned i64 %a %thirteen
  %x1 = xor i64 %a %s1
  %seven_shift = const i64 7
  %s2 = shl i64 %x1 %seven_shift
  %x2 = xor i64 %x1 %s2
  %next = add i64 %j %one
  jump loop(%next, %x2)
exit(%result: i64):
  return %result
}

func @forge_fib(%n: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %zero, %one)
loop(%i: i64, %a: i64, %b: i64):
  %done = cmp.ge i64 %i %n
  branch %done, exit(%a), body(%i, %a, %b)
body(%j: i64, %x: i64, %y: i64):
  %sum = add i64 %x %y
  %next = add i64 %j %one
  jump loop(%next, %y, %sum)
exit(%result: i64):
  return %result
}

func @forge_branch_walk(%seed: i64, %rounds: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %seed)
loop(%i: i64, %x: i64):
  %done = cmp.ge i64 %i %rounds
  branch %done, exit(%x), body(%i, %x)
body(%j: i64, %v: i64):
  %mask = const i64 1
  %bit = and i64 %v %mask
  %odd = cmp.ne i64 %bit %zero
  branch %odd, odd_path(%j, %v), even_path(%j, %v)
odd_path(%j1: i64, %v1: i64):
  %three = const i64 3
  %seven = const i64 7
  %mul = mul i64 %v1 %three
  %nv = add i64 %mul %seven
  %ni = add i64 %j1 %one
  jump loop(%ni, %nv)
even_path(%j2: i64, %v2: i64):
  %shift = shr.unsigned i64 %v2 %one
  %five = const i64 5
  %nv2 = xor i64 %shift %five
  %ni2 = add i64 %j2 %one
  jump loop(%ni2, %nv2)
exit(%result: i64):
  return %result
}

func @forge_reg_pressure(%a: i64, %b: i64, %c: i64, %d: i64, %e: i64, %f: i64, %g: i64, %h: i64) -> i64 {
entry:
  %ab = add i64 %a %b
  %cd = add i64 %c %d
  %ef = add i64 %e %f
  %gh = add i64 %g %h
  %p0 = mul i64 %ab %cd
  %p1 = mul i64 %ef %gh
  %x = xor i64 %p0 %p1
  %r0 = add i64 %x %ab
  %r1 = add i64 %r0 %ef
  return %r1
}

func @forge_float_poly(%x: f64, %rounds: i64) -> f64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %x)
loop(%i: i64, %v: f64):
  %done = cmp.ge i64 %i %rounds
  branch %done, exit(%v), body(%i, %v)
body(%j: i64, %y: f64):
  %c1 = const f64 1.0000001192092896
  %c2 = const f64 0.00000095367431640625
  %m = mul f64 %y %c1
  %nv = add f64 %m %c2
  %next = add i64 %j %one
  jump loop(%next, %nv)
exit(%result: f64):
  return %result
}

func @forge_memory4(%base: ptr) -> i64 {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %a = load i64 %base align 8
  %b = load i64 %p1 align 8
  %c = load i64 %p2 align 8
  %d = load i64 %p3 align 8
  %ab = add i64 %a %b
  %cd = add i64 %c %d
  %sum = add i64 %ab %cd
  return %sum
}

func @forge_call_leaf(%x: i64, %y: i64, %z: i64) -> i64 {
entry:
  %xy = add i64 %x %y
  %m = mul i64 %xy %z
  %seven = const i64 7
  %r = xor i64 %m %seven
  return %r
}

func @forge_call_chain(%x: i64) -> i64 {
entry:
  %two = const i64 2
  %three = const i64 3
  %a = call i64 @forge_call_leaf(%x, %two, %three)
  %b = call i64 @forge_call_leaf(%a, %three, %two)
  %c = call i64 @forge_call_leaf(%b, %two, %three)
  %d = call i64 @forge_call_leaf(%c, %three, %two)
  return %d
}

func @forge_float_leaf(%x: f64, %y: f64) -> f64 {
entry:
  %m = mul f64 %x %y
  %c = const f64 0.125
  %r = add f64 %m %c
  return %r
}

func @forge_float_calls(%x: f64) -> f64 {
entry:
  %a = const f64 1.5
  %b = const f64 0.75
  %r0 = call f64 @forge_float_leaf(%x, %a)
  %r1 = call f64 @forge_float_leaf(%r0, %b)
  %r2 = call f64 @forge_float_leaf(%r1, %a)
  return %r2
}

func @forge_memory_sum(%base: ptr) -> i64 {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %p4 = ptr.offset ptr %base 32
  %p5 = ptr.offset ptr %base 40
  %p6 = ptr.offset ptr %base 48
  %p7 = ptr.offset ptr %base 56
  %v0 = load i64 %base align 8
  %v1 = load i64 %p1 align 8
  %v2 = load i64 %p2 align 8
  %v3 = load i64 %p3 align 8
  %v4 = load i64 %p4 align 8
  %v5 = load i64 %p5 align 8
  %v6 = load i64 %p6 align 8
  %v7 = load i64 %p7 align 8
  %a0 = add i64 %v0 %v1
  %a1 = add i64 %v2 %v3
  %a2 = add i64 %v4 %v5
  %a3 = add i64 %v6 %v7
  %b0 = add i64 %a0 %a1
  %b1 = add i64 %a2 %a3
  %sum = add i64 %b0 %b1
  return %sum
}
func @forge_memory_update4(%base: ptr, %delta: i64) -> i64 {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %a = load i64 %base align 8
  %b = load i64 %p1 align 8
  %c = load i64 %p2 align 8
  %d = load i64 %p3 align 8
  %na = add i64 %a %delta
  %nb = add i64 %b %delta
  %nc = add i64 %c %delta
  %nd = add i64 %d %delta
  store i64 %na %base align 8
  store i64 %nb %p1 align 8
  store i64 %nc %p2 align 8
  store i64 %nd %p3 align 8
  %ab = add i64 %na %nb
  %cd = add i64 %nc %nd
  %sum = add i64 %ab %cd
  return %sum
}

func @forge_call_live(%x: i64) -> i64 {
entry:
  %two = const i64 2
  %three = const i64 3
  %called = call i64 @forge_call_leaf(%x, %two, %three)
  %sum = add i64 %called %x
  return %sum
}



func @forge_multi_recurrence(%n: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  %two = const i64 2
  %three = const i64 3
  jump loop(%zero, %one, %two, %three, %one)
loop(%i: i64, %a: i64, %b: i64, %c: i64, %d: i64):
  %done = cmp.ge i64 %i %n
  branch %done, exit(%a, %b, %c, %d), body(%i, %a, %b, %c, %d)
body(%j: i64, %w: i64, %x: i64, %y: i64, %z: i64):
  %wx = add i64 %w %x
  %yz = add i64 %y %z
  %next_value = xor i64 %wx %yz
  %next_i = add i64 %j %one
  jump loop(%next_i, %x, %y, %z, %next_value)
exit(%ra: i64, %rb: i64, %rc: i64, %rd: i64):
  %ab = add i64 %ra %rb
  %cd = add i64 %rc %rd
  %result = add i64 %ab %cd
  return %result
}

func @forge_branch_merge(%seed: i64, %rounds: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %seed)
loop(%i: i64, %value: i64):
  %done = cmp.ge i64 %i %rounds
  branch %done, exit(%value), choose(%i, %value)
choose(%j: i64, %v: i64):
  %mask = const i64 8
  %masked = and i64 %v %mask
  %set = cmp.ne i64 %masked %zero
  branch %set, high(%j, %v), low(%j, %v)
high(%jh: i64, %vh: i64):
  %five = const i64 5
  %hv = add i64 %vh %five
  jump merge(%jh, %hv)
low(%jl: i64, %vl: i64):
  %three = const i64 3
  %lv = xor i64 %vl %three
  jump merge(%jl, %lv)
merge(%jm: i64, %merged: i64):
  %next_i = add i64 %jm %one
  jump loop(%next_i, %merged)
exit(%result: i64):
  return %result
}

func @forge_store_overwrite(%base: ptr, %value: i64) -> i64 {
entry:
  store i64 %value %base align 8
  %one = const i64 1
  %next = add i64 %value %one
  store i64 %next %base align 8
  %result = load i64 %base align 8
  return %result
}

func @forge_global_store_overwrite(%base: ptr, %value: i64) -> i64 {
entry:
  store i64 %value %base align 8
  jump overwrite()
overwrite:
  %one = const i64 1
  %next = add i64 %value %one
  store i64 %next %base align 8
  jump exit()
exit:
  %result = load i64 %base align 8
  return %result
}

func @forge_float_dot4(%a: ptr, %b: ptr) -> f64 {
entry:
  %a1p = ptr.offset ptr %a 8
  %a2p = ptr.offset ptr %a 16
  %a3p = ptr.offset ptr %a 24
  %b1p = ptr.offset ptr %b 8
  %b2p = ptr.offset ptr %b 16
  %b3p = ptr.offset ptr %b 24
  %a0 = load f64 %a align 8
  %a1 = load f64 %a1p align 8
  %a2 = load f64 %a2p align 8
  %a3 = load f64 %a3p align 8
  %b0 = load f64 %b align 8
  %b1 = load f64 %b1p align 8
  %b2 = load f64 %b2p align 8
  %b3 = load f64 %b3p align 8
  %p0 = mul f64 %a0 %b0
  %p1 = mul f64 %a1 %b1
  %p2 = mul f64 %a2 %b2
  %p3 = mul f64 %a3 %b3
  %s0 = add f64 %p0 %p1
  %s1 = add f64 %p2 %p3
  %result = add f64 %s0 %s1
  return %result
}

}
