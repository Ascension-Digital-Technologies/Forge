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
func @forge_memory_sum_i32(%base: ptr) -> i32 {
entry:
  %p1 = ptr.offset ptr %base 4
  %p2 = ptr.offset ptr %base 8
  %p3 = ptr.offset ptr %base 12
  %p4 = ptr.offset ptr %base 16
  %p5 = ptr.offset ptr %base 20
  %p6 = ptr.offset ptr %base 24
  %p7 = ptr.offset ptr %base 28
  %v0 = load i32 %base align 4
  %v1 = load i32 %p1 align 4
  %v2 = load i32 %p2 align 4
  %v3 = load i32 %p3 align 4
  %v4 = load i32 %p4 align 4
  %v5 = load i32 %p5 align 4
  %v6 = load i32 %p6 align 4
  %v7 = load i32 %p7 align 4
  %a0 = add i32 %v0 %v1
  %a1 = add i32 %v2 %v3
  %a2 = add i32 %v4 %v5
  %a3 = add i32 %v6 %v7
  %b0 = add i32 %a0 %a1
  %b1 = add i32 %a2 %a3
  %sum = add i32 %b0 %b1
  return %sum
}

func @forge_memory_add4(%base: ptr, %delta: i64) -> void {
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
  return
}

func @forge_memory_xor4_i32(%base: ptr, %mask: i32) -> void {
entry:
  %p1 = ptr.offset ptr %base 4
  %p2 = ptr.offset ptr %base 8
  %p3 = ptr.offset ptr %base 12
  %a = load i32 %base align 4
  %b = load i32 %p1 align 4
  %c = load i32 %p2 align 4
  %d = load i32 %p3 align 4
  %na = xor i32 %a %mask
  %nb = xor i32 %b %mask
  %nc = xor i32 %c %mask
  %nd = xor i32 %d %mask
  store i32 %na %base align 4
  store i32 %nb %p1 align 4
  store i32 %nc %p2 align 4
  store i32 %nd %p3 align 4
  return
}

func @forge_memory_and4_i64(%base: ptr, %mask: i64) -> void {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %a = load i64 %base align 8
  %b = load i64 %p1 align 8
  %c = load i64 %p2 align 8
  %d = load i64 %p3 align 8
  %na = and i64 %a %mask
  %nb = and i64 %b %mask
  %nc = and i64 %c %mask
  %nd = and i64 %d %mask
  store i64 %na %base align 8
  store i64 %nb %p1 align 8
  store i64 %nc %p2 align 8
  store i64 %nd %p3 align 8
  return
}

func @forge_memory_xor_copy8_i32(%src: ptr, %dst: ptr, %mask: i32) -> void {
entry:
  %s1 = ptr.offset ptr %src 4
  %s2 = ptr.offset ptr %src 8
  %s3 = ptr.offset ptr %src 12
  %s4 = ptr.offset ptr %src 16
  %s5 = ptr.offset ptr %src 20
  %s6 = ptr.offset ptr %src 24
  %s7 = ptr.offset ptr %src 28
  %d1 = ptr.offset ptr %dst 4
  %d2 = ptr.offset ptr %dst 8
  %d3 = ptr.offset ptr %dst 12
  %d4 = ptr.offset ptr %dst 16
  %d5 = ptr.offset ptr %dst 20
  %d6 = ptr.offset ptr %dst 24
  %d7 = ptr.offset ptr %dst 28
  %v0 = load i32 %src align 4
  %v1 = load i32 %s1 align 4
  %v2 = load i32 %s2 align 4
  %v3 = load i32 %s3 align 4
  %v4 = load i32 %s4 align 4
  %v5 = load i32 %s5 align 4
  %v6 = load i32 %s6 align 4
  %v7 = load i32 %s7 align 4
  %r0 = xor i32 %v0 %mask
  %r1 = xor i32 %v1 %mask
  %r2 = xor i32 %v2 %mask
  %r3 = xor i32 %v3 %mask
  %r4 = xor i32 %v4 %mask
  %r5 = xor i32 %v5 %mask
  %r6 = xor i32 %v6 %mask
  %r7 = xor i32 %v7 %mask
  store i32 %r0 %dst align 4
  store i32 %r1 %d1 align 4
  store i32 %r2 %d2 align 4
  store i32 %r3 %d3 align 4
  store i32 %r4 %d4 align 4
  store i32 %r5 %d5 align 4
  store i32 %r6 %d6 align 4
  store i32 %r7 %d7 align 4
  return
}

func @forge_memory_add_copy8_i32(%lhs: ptr, %rhs: ptr, %dst: ptr) -> void {
entry:
  %l1 = ptr.offset ptr %lhs 4
  %l2 = ptr.offset ptr %lhs 8
  %l3 = ptr.offset ptr %lhs 12
  %l4 = ptr.offset ptr %lhs 16
  %l5 = ptr.offset ptr %lhs 20
  %l6 = ptr.offset ptr %lhs 24
  %l7 = ptr.offset ptr %lhs 28
  %r1 = ptr.offset ptr %rhs 4
  %r2 = ptr.offset ptr %rhs 8
  %r3 = ptr.offset ptr %rhs 12
  %r4 = ptr.offset ptr %rhs 16
  %r5 = ptr.offset ptr %rhs 20
  %r6 = ptr.offset ptr %rhs 24
  %r7 = ptr.offset ptr %rhs 28
  %d1 = ptr.offset ptr %dst 4
  %d2 = ptr.offset ptr %dst 8
  %d3 = ptr.offset ptr %dst 12
  %d4 = ptr.offset ptr %dst 16
  %d5 = ptr.offset ptr %dst 20
  %d6 = ptr.offset ptr %dst 24
  %d7 = ptr.offset ptr %dst 28
  %a0 = load i32 %lhs align 4
  %a1 = load i32 %l1 align 4
  %a2 = load i32 %l2 align 4
  %a3 = load i32 %l3 align 4
  %a4 = load i32 %l4 align 4
  %a5 = load i32 %l5 align 4
  %a6 = load i32 %l6 align 4
  %a7 = load i32 %l7 align 4
  %b0 = load i32 %rhs align 4
  %b1 = load i32 %r1 align 4
  %b2 = load i32 %r2 align 4
  %b3 = load i32 %r3 align 4
  %b4 = load i32 %r4 align 4
  %b5 = load i32 %r5 align 4
  %b6 = load i32 %r6 align 4
  %b7 = load i32 %r7 align 4
  %v0 = add i32 %a0 %b0
  %v1 = add i32 %a1 %b1
  %v2 = add i32 %a2 %b2
  %v3 = add i32 %a3 %b3
  %v4 = add i32 %a4 %b4
  %v5 = add i32 %a5 %b5
  %v6 = add i32 %a6 %b6
  %v7 = add i32 %a7 %b7
  store i32 %v0 %dst align 4
  store i32 %v1 %d1 align 4
  store i32 %v2 %d2 align 4
  store i32 %v3 %d3 align 4
  store i32 %v4 %d4 align 4
  store i32 %v5 %d5 align 4
  store i32 %v6 %d6 align 4
  store i32 %v7 %d7 align 4
  return
}


func @forge_memory_chain_copy8_i32(%a: ptr, %b: ptr, %c: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %a4 = ptr.offset ptr %a 16
  %a5 = ptr.offset ptr %a 20
  %a6 = ptr.offset ptr %a 24
  %a7 = ptr.offset ptr %a 28
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %b4 = ptr.offset ptr %b 16
  %b5 = ptr.offset ptr %b 20
  %b6 = ptr.offset ptr %b 24
  %b7 = ptr.offset ptr %b 28
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %c4 = ptr.offset ptr %c 16
  %c5 = ptr.offset ptr %c 20
  %c6 = ptr.offset ptr %c 24
  %c7 = ptr.offset ptr %c 28
  %d1 = ptr.offset ptr %dst 4
  %d2 = ptr.offset ptr %dst 8
  %d3 = ptr.offset ptr %dst 12
  %d4 = ptr.offset ptr %dst 16
  %d5 = ptr.offset ptr %dst 20
  %d6 = ptr.offset ptr %dst 24
  %d7 = ptr.offset ptr %dst 28
  %av0 = load i32 %a align 4
  %av1 = load i32 %a1 align 4
  %av2 = load i32 %a2 align 4
  %av3 = load i32 %a3 align 4
  %av4 = load i32 %a4 align 4
  %av5 = load i32 %a5 align 4
  %av6 = load i32 %a6 align 4
  %av7 = load i32 %a7 align 4
  %bv0 = load i32 %b align 4
  %bv1 = load i32 %b1 align 4
  %bv2 = load i32 %b2 align 4
  %bv3 = load i32 %b3 align 4
  %bv4 = load i32 %b4 align 4
  %bv5 = load i32 %b5 align 4
  %bv6 = load i32 %b6 align 4
  %bv7 = load i32 %b7 align 4
  %cv0 = load i32 %c align 4
  %cv1 = load i32 %c1 align 4
  %cv2 = load i32 %c2 align 4
  %cv3 = load i32 %c3 align 4
  %cv4 = load i32 %c4 align 4
  %cv5 = load i32 %c5 align 4
  %cv6 = load i32 %c6 align 4
  %cv7 = load i32 %c7 align 4
  %x0 = xor i32 %av0 %bv0
  %x1 = xor i32 %av1 %bv1
  %x2 = xor i32 %av2 %bv2
  %x3 = xor i32 %av3 %bv3
  %x4 = xor i32 %av4 %bv4
  %x5 = xor i32 %av5 %bv5
  %x6 = xor i32 %av6 %bv6
  %x7 = xor i32 %av7 %bv7
  %v0 = add i32 %x0 %cv0
  %v1 = add i32 %x1 %cv1
  %v2 = add i32 %x2 %cv2
  %v3 = add i32 %x3 %cv3
  %v4 = add i32 %x4 %cv4
  %v5 = add i32 %x5 %cv5
  %v6 = add i32 %x6 %cv6
  %v7 = add i32 %x7 %cv7
  store i32 %v0 %dst align 4
  store i32 %v1 %d1 align 4
  store i32 %v2 %d2 align 4
  store i32 %v3 %d3 align 4
  store i32 %v4 %d4 align 4
  store i32 %v5 %d5 align 4
  store i32 %v6 %d6 align 4
  store i32 %v7 %d7 align 4
  return
}

func @forge_memory_deep_chain_copy8_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %a4 = ptr.offset ptr %a 16
  %a5 = ptr.offset ptr %a 20
  %a6 = ptr.offset ptr %a 24
  %a7 = ptr.offset ptr %a 28
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %b4 = ptr.offset ptr %b 16
  %b5 = ptr.offset ptr %b 20
  %b6 = ptr.offset ptr %b 24
  %b7 = ptr.offset ptr %b 28
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %c4 = ptr.offset ptr %c 16
  %c5 = ptr.offset ptr %c 20
  %c6 = ptr.offset ptr %c 24
  %c7 = ptr.offset ptr %c 28
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %d4 = ptr.offset ptr %d 16
  %d5 = ptr.offset ptr %d 20
  %d6 = ptr.offset ptr %d 24
  %d7 = ptr.offset ptr %d 28
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %e4 = ptr.offset ptr %e 16
  %e5 = ptr.offset ptr %e 20
  %e6 = ptr.offset ptr %e 24
  %e7 = ptr.offset ptr %e 28
  %dst1 = ptr.offset ptr %dst 4
  %dst2 = ptr.offset ptr %dst 8
  %dst3 = ptr.offset ptr %dst 12
  %dst4 = ptr.offset ptr %dst 16
  %dst5 = ptr.offset ptr %dst 20
  %dst6 = ptr.offset ptr %dst 24
  %dst7 = ptr.offset ptr %dst 28
  %av0 = load i32 %a align 4
  %av1 = load i32 %a1 align 4
  %av2 = load i32 %a2 align 4
  %av3 = load i32 %a3 align 4
  %av4 = load i32 %a4 align 4
  %av5 = load i32 %a5 align 4
  %av6 = load i32 %a6 align 4
  %av7 = load i32 %a7 align 4
  %bv0 = load i32 %b align 4
  %bv1 = load i32 %b1 align 4
  %bv2 = load i32 %b2 align 4
  %bv3 = load i32 %b3 align 4
  %bv4 = load i32 %b4 align 4
  %bv5 = load i32 %b5 align 4
  %bv6 = load i32 %b6 align 4
  %bv7 = load i32 %b7 align 4
  %cv0 = load i32 %c align 4
  %cv1 = load i32 %c1 align 4
  %cv2 = load i32 %c2 align 4
  %cv3 = load i32 %c3 align 4
  %cv4 = load i32 %c4 align 4
  %cv5 = load i32 %c5 align 4
  %cv6 = load i32 %c6 align 4
  %cv7 = load i32 %c7 align 4
  %dv0 = load i32 %d align 4
  %dv1 = load i32 %d1 align 4
  %dv2 = load i32 %d2 align 4
  %dv3 = load i32 %d3 align 4
  %dv4 = load i32 %d4 align 4
  %dv5 = load i32 %d5 align 4
  %dv6 = load i32 %d6 align 4
  %dv7 = load i32 %d7 align 4
  %ev0 = load i32 %e align 4
  %ev1 = load i32 %e1 align 4
  %ev2 = load i32 %e2 align 4
  %ev3 = load i32 %e3 align 4
  %ev4 = load i32 %e4 align 4
  %ev5 = load i32 %e5 align 4
  %ev6 = load i32 %e6 align 4
  %ev7 = load i32 %e7 align 4
  %x0 = xor i32 %av0 %bv0
  %x1 = xor i32 %av1 %bv1
  %x2 = xor i32 %av2 %bv2
  %x3 = xor i32 %av3 %bv3
  %x4 = xor i32 %av4 %bv4
  %x5 = xor i32 %av5 %bv5
  %x6 = xor i32 %av6 %bv6
  %x7 = xor i32 %av7 %bv7
  %y0 = add i32 %x0 %cv0
  %y1 = add i32 %x1 %cv1
  %y2 = add i32 %x2 %cv2
  %y3 = add i32 %x3 %cv3
  %y4 = add i32 %x4 %cv4
  %y5 = add i32 %x5 %cv5
  %y6 = add i32 %x6 %cv6
  %y7 = add i32 %x7 %cv7
  %z0 = and i32 %y0 %dv0
  %z1 = and i32 %y1 %dv1
  %z2 = and i32 %y2 %dv2
  %z3 = and i32 %y3 %dv3
  %z4 = and i32 %y4 %dv4
  %z5 = and i32 %y5 %dv5
  %z6 = and i32 %y6 %dv6
  %z7 = and i32 %y7 %dv7
  %v0 = sub i32 %z0 %ev0
  %v1 = sub i32 %z1 %ev1
  %v2 = sub i32 %z2 %ev2
  %v3 = sub i32 %z3 %ev3
  %v4 = sub i32 %z4 %ev4
  %v5 = sub i32 %z5 %ev5
  %v6 = sub i32 %z6 %ev6
  %v7 = sub i32 %z7 %ev7
  store i32 %v0 %dst align 4
  store i32 %v1 %dst1 align 4
  store i32 %v2 %dst2 align 4
  store i32 %v3 %dst3 align 4
  store i32 %v4 %dst4 align 4
  store i32 %v5 %dst5 align 4
  store i32 %v6 %dst6 align 4
  store i32 %v7 %dst7 align 4
  return
}

func @forge_memory_branch_dag_copy8_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %a4 = ptr.offset ptr %a 16
  %a5 = ptr.offset ptr %a 20
  %a6 = ptr.offset ptr %a 24
  %a7 = ptr.offset ptr %a 28
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %b4 = ptr.offset ptr %b 16
  %b5 = ptr.offset ptr %b 20
  %b6 = ptr.offset ptr %b 24
  %b7 = ptr.offset ptr %b 28
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %c4 = ptr.offset ptr %c 16
  %c5 = ptr.offset ptr %c 20
  %c6 = ptr.offset ptr %c 24
  %c7 = ptr.offset ptr %c 28
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %d4 = ptr.offset ptr %d 16
  %d5 = ptr.offset ptr %d 20
  %d6 = ptr.offset ptr %d 24
  %d7 = ptr.offset ptr %d 28
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %e4 = ptr.offset ptr %e 16
  %e5 = ptr.offset ptr %e 20
  %e6 = ptr.offset ptr %e 24
  %e7 = ptr.offset ptr %e 28
  %dst1 = ptr.offset ptr %dst 4
  %dst2 = ptr.offset ptr %dst 8
  %dst3 = ptr.offset ptr %dst 12
  %dst4 = ptr.offset ptr %dst 16
  %dst5 = ptr.offset ptr %dst 20
  %dst6 = ptr.offset ptr %dst 24
  %dst7 = ptr.offset ptr %dst 28
  %av0 = load i32 %a align 4
  %av1 = load i32 %a1 align 4
  %av2 = load i32 %a2 align 4
  %av3 = load i32 %a3 align 4
  %av4 = load i32 %a4 align 4
  %av5 = load i32 %a5 align 4
  %av6 = load i32 %a6 align 4
  %av7 = load i32 %a7 align 4
  %bv0 = load i32 %b align 4
  %bv1 = load i32 %b1 align 4
  %bv2 = load i32 %b2 align 4
  %bv3 = load i32 %b3 align 4
  %bv4 = load i32 %b4 align 4
  %bv5 = load i32 %b5 align 4
  %bv6 = load i32 %b6 align 4
  %bv7 = load i32 %b7 align 4
  %cv0 = load i32 %c align 4
  %cv1 = load i32 %c1 align 4
  %cv2 = load i32 %c2 align 4
  %cv3 = load i32 %c3 align 4
  %cv4 = load i32 %c4 align 4
  %cv5 = load i32 %c5 align 4
  %cv6 = load i32 %c6 align 4
  %cv7 = load i32 %c7 align 4
  %dv0 = load i32 %d align 4
  %dv1 = load i32 %d1 align 4
  %dv2 = load i32 %d2 align 4
  %dv3 = load i32 %d3 align 4
  %dv4 = load i32 %d4 align 4
  %dv5 = load i32 %d5 align 4
  %dv6 = load i32 %d6 align 4
  %dv7 = load i32 %d7 align 4
  %ev0 = load i32 %e align 4
  %ev1 = load i32 %e1 align 4
  %ev2 = load i32 %e2 align 4
  %ev3 = load i32 %e3 align 4
  %ev4 = load i32 %e4 align 4
  %ev5 = load i32 %e5 align 4
  %ev6 = load i32 %e6 align 4
  %ev7 = load i32 %e7 align 4
  %x0 = xor i32 %av0 %bv0
  %x1 = xor i32 %av1 %bv1
  %x2 = xor i32 %av2 %bv2
  %x3 = xor i32 %av3 %bv3
  %x4 = xor i32 %av4 %bv4
  %x5 = xor i32 %av5 %bv5
  %x6 = xor i32 %av6 %bv6
  %x7 = xor i32 %av7 %bv7
  %y0 = and i32 %cv0 %dv0
  %y1 = and i32 %cv1 %dv1
  %y2 = and i32 %cv2 %dv2
  %y3 = and i32 %cv3 %dv3
  %y4 = and i32 %cv4 %dv4
  %y5 = and i32 %cv5 %dv5
  %y6 = and i32 %cv6 %dv6
  %y7 = and i32 %cv7 %dv7
  %z0 = add i32 %x0 %y0
  %z1 = add i32 %x1 %y1
  %z2 = add i32 %x2 %y2
  %z3 = add i32 %x3 %y3
  %z4 = add i32 %x4 %y4
  %z5 = add i32 %x5 %y5
  %z6 = add i32 %x6 %y6
  %z7 = add i32 %x7 %y7
  %v0 = sub i32 %z0 %ev0
  %v1 = sub i32 %z1 %ev1
  %v2 = sub i32 %z2 %ev2
  %v3 = sub i32 %z3 %ev3
  %v4 = sub i32 %z4 %ev4
  %v5 = sub i32 %z5 %ev5
  %v6 = sub i32 %z6 %ev6
  %v7 = sub i32 %z7 %ev7
  store i32 %v0 %dst align 4
  store i32 %v1 %dst1 align 4
  store i32 %v2 %dst2 align 4
  store i32 %v3 %dst3 align 4
  store i32 %v4 %dst4 align 4
  store i32 %v5 %dst5 align 4
  store i32 %v6 %dst6 align 4
  store i32 %v7 %dst7 align 4
  return
}

func @forge_memory_shared_dag_copy8_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %a4 = ptr.offset ptr %a 16
  %a5 = ptr.offset ptr %a 20
  %a6 = ptr.offset ptr %a 24
  %a7 = ptr.offset ptr %a 28
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %b4 = ptr.offset ptr %b 16
  %b5 = ptr.offset ptr %b 20
  %b6 = ptr.offset ptr %b 24
  %b7 = ptr.offset ptr %b 28
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %c4 = ptr.offset ptr %c 16
  %c5 = ptr.offset ptr %c 20
  %c6 = ptr.offset ptr %c 24
  %c7 = ptr.offset ptr %c 28
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %d4 = ptr.offset ptr %d 16
  %d5 = ptr.offset ptr %d 20
  %d6 = ptr.offset ptr %d 24
  %d7 = ptr.offset ptr %d 28
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %e4 = ptr.offset ptr %e 16
  %e5 = ptr.offset ptr %e 20
  %e6 = ptr.offset ptr %e 24
  %e7 = ptr.offset ptr %e 28
  %dst1 = ptr.offset ptr %dst 4
  %dst2 = ptr.offset ptr %dst 8
  %dst3 = ptr.offset ptr %dst 12
  %dst4 = ptr.offset ptr %dst 16
  %dst5 = ptr.offset ptr %dst 20
  %dst6 = ptr.offset ptr %dst 24
  %dst7 = ptr.offset ptr %dst 28
  %av0 = load i32 %a align 4
  %av1 = load i32 %a1 align 4
  %av2 = load i32 %a2 align 4
  %av3 = load i32 %a3 align 4
  %av4 = load i32 %a4 align 4
  %av5 = load i32 %a5 align 4
  %av6 = load i32 %a6 align 4
  %av7 = load i32 %a7 align 4
  %bv0 = load i32 %b align 4
  %bv1 = load i32 %b1 align 4
  %bv2 = load i32 %b2 align 4
  %bv3 = load i32 %b3 align 4
  %bv4 = load i32 %b4 align 4
  %bv5 = load i32 %b5 align 4
  %bv6 = load i32 %b6 align 4
  %bv7 = load i32 %b7 align 4
  %cv0 = load i32 %c align 4
  %cv1 = load i32 %c1 align 4
  %cv2 = load i32 %c2 align 4
  %cv3 = load i32 %c3 align 4
  %cv4 = load i32 %c4 align 4
  %cv5 = load i32 %c5 align 4
  %cv6 = load i32 %c6 align 4
  %cv7 = load i32 %c7 align 4
  %dv0 = load i32 %d align 4
  %dv1 = load i32 %d1 align 4
  %dv2 = load i32 %d2 align 4
  %dv3 = load i32 %d3 align 4
  %dv4 = load i32 %d4 align 4
  %dv5 = load i32 %d5 align 4
  %dv6 = load i32 %d6 align 4
  %dv7 = load i32 %d7 align 4
  %ev0 = load i32 %e align 4
  %ev1 = load i32 %e1 align 4
  %ev2 = load i32 %e2 align 4
  %ev3 = load i32 %e3 align 4
  %ev4 = load i32 %e4 align 4
  %ev5 = load i32 %e5 align 4
  %ev6 = load i32 %e6 align 4
  %ev7 = load i32 %e7 align 4
  %s0 = xor i32 %av0 %bv0
  %s1 = xor i32 %av1 %bv1
  %s2 = xor i32 %av2 %bv2
  %s3 = xor i32 %av3 %bv3
  %s4 = xor i32 %av4 %bv4
  %s5 = xor i32 %av5 %bv5
  %s6 = xor i32 %av6 %bv6
  %s7 = xor i32 %av7 %bv7
  %l0 = add i32 %s0 %cv0
  %l1 = add i32 %s1 %cv1
  %l2 = add i32 %s2 %cv2
  %l3 = add i32 %s3 %cv3
  %l4 = add i32 %s4 %cv4
  %l5 = add i32 %s5 %cv5
  %l6 = add i32 %s6 %cv6
  %l7 = add i32 %s7 %cv7
  %r0 = and i32 %s0 %dv0
  %r1 = and i32 %s1 %dv1
  %r2 = and i32 %s2 %dv2
  %r3 = and i32 %s3 %dv3
  %r4 = and i32 %s4 %dv4
  %r5 = and i32 %s5 %dv5
  %r6 = and i32 %s6 %dv6
  %r7 = and i32 %s7 %dv7
  %m0 = add i32 %l0 %r0
  %m1 = add i32 %l1 %r1
  %m2 = add i32 %l2 %r2
  %m3 = add i32 %l3 %r3
  %m4 = add i32 %l4 %r4
  %m5 = add i32 %l5 %r5
  %m6 = add i32 %l6 %r6
  %m7 = add i32 %l7 %r7
  %v0 = xor i32 %m0 %ev0
  %v1 = xor i32 %m1 %ev1
  %v2 = xor i32 %m2 %ev2
  %v3 = xor i32 %m3 %ev3
  %v4 = xor i32 %m4 %ev4
  %v5 = xor i32 %m5 %ev5
  %v6 = xor i32 %m6 %ev6
  %v7 = xor i32 %m7 %ev7
  store i32 %v0 %dst align 4
  store i32 %v1 %dst1 align 4
  store i32 %v2 %dst2 align 4
  store i32 %v3 %dst3 align 4
  store i32 %v4 %dst4 align 4
  store i32 %v5 %dst5 align 4
  store i32 %v6 %dst6 align 4
  store i32 %v7 %dst7 align 4
  return
}

func @forge_memory_pressure_dag_copy8_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %a4 = ptr.offset ptr %a 16
  %a5 = ptr.offset ptr %a 20
  %a6 = ptr.offset ptr %a 24
  %a7 = ptr.offset ptr %a 28
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %b4 = ptr.offset ptr %b 16
  %b5 = ptr.offset ptr %b 20
  %b6 = ptr.offset ptr %b 24
  %b7 = ptr.offset ptr %b 28
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %c4 = ptr.offset ptr %c 16
  %c5 = ptr.offset ptr %c 20
  %c6 = ptr.offset ptr %c 24
  %c7 = ptr.offset ptr %c 28
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %d4 = ptr.offset ptr %d 16
  %d5 = ptr.offset ptr %d 20
  %d6 = ptr.offset ptr %d 24
  %d7 = ptr.offset ptr %d 28
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %e4 = ptr.offset ptr %e 16
  %e5 = ptr.offset ptr %e 20
  %e6 = ptr.offset ptr %e 24
  %e7 = ptr.offset ptr %e 28
  %dst1 = ptr.offset ptr %dst 4
  %dst2 = ptr.offset ptr %dst 8
  %dst3 = ptr.offset ptr %dst 12
  %dst4 = ptr.offset ptr %dst 16
  %dst5 = ptr.offset ptr %dst 20
  %dst6 = ptr.offset ptr %dst 24
  %dst7 = ptr.offset ptr %dst 28
  %av0 = load i32 %a align 4
  %av1 = load i32 %a1 align 4
  %av2 = load i32 %a2 align 4
  %av3 = load i32 %a3 align 4
  %av4 = load i32 %a4 align 4
  %av5 = load i32 %a5 align 4
  %av6 = load i32 %a6 align 4
  %av7 = load i32 %a7 align 4
  %bv0 = load i32 %b align 4
  %bv1 = load i32 %b1 align 4
  %bv2 = load i32 %b2 align 4
  %bv3 = load i32 %b3 align 4
  %bv4 = load i32 %b4 align 4
  %bv5 = load i32 %b5 align 4
  %bv6 = load i32 %b6 align 4
  %bv7 = load i32 %b7 align 4
  %cv0 = load i32 %c align 4
  %cv1 = load i32 %c1 align 4
  %cv2 = load i32 %c2 align 4
  %cv3 = load i32 %c3 align 4
  %cv4 = load i32 %c4 align 4
  %cv5 = load i32 %c5 align 4
  %cv6 = load i32 %c6 align 4
  %cv7 = load i32 %c7 align 4
  %dv0 = load i32 %d align 4
  %dv1 = load i32 %d1 align 4
  %dv2 = load i32 %d2 align 4
  %dv3 = load i32 %d3 align 4
  %dv4 = load i32 %d4 align 4
  %dv5 = load i32 %d5 align 4
  %dv6 = load i32 %d6 align 4
  %dv7 = load i32 %d7 align 4
  %ev0 = load i32 %e align 4
  %ev1 = load i32 %e1 align 4
  %ev2 = load i32 %e2 align 4
  %ev3 = load i32 %e3 align 4
  %ev4 = load i32 %e4 align 4
  %ev5 = load i32 %e5 align 4
  %ev6 = load i32 %e6 align 4
  %ev7 = load i32 %e7 align 4
  %s0_0 = xor i32 %av0 %bv0
  %s0_1 = xor i32 %av1 %bv1
  %s0_2 = xor i32 %av2 %bv2
  %s0_3 = xor i32 %av3 %bv3
  %s0_4 = xor i32 %av4 %bv4
  %s0_5 = xor i32 %av5 %bv5
  %s0_6 = xor i32 %av6 %bv6
  %s0_7 = xor i32 %av7 %bv7
  %s1_0 = and i32 %av0 %bv0
  %s1_1 = and i32 %av1 %bv1
  %s1_2 = and i32 %av2 %bv2
  %s1_3 = and i32 %av3 %bv3
  %s1_4 = and i32 %av4 %bv4
  %s1_5 = and i32 %av5 %bv5
  %s1_6 = and i32 %av6 %bv6
  %s1_7 = and i32 %av7 %bv7
  %s2_0 = or i32 %av0 %bv0
  %s2_1 = or i32 %av1 %bv1
  %s2_2 = or i32 %av2 %bv2
  %s2_3 = or i32 %av3 %bv3
  %s2_4 = or i32 %av4 %bv4
  %s2_5 = or i32 %av5 %bv5
  %s2_6 = or i32 %av6 %bv6
  %s2_7 = or i32 %av7 %bv7
  %s3_0 = add i32 %av0 %bv0
  %s3_1 = add i32 %av1 %bv1
  %s3_2 = add i32 %av2 %bv2
  %s3_3 = add i32 %av3 %bv3
  %s3_4 = add i32 %av4 %bv4
  %s3_5 = add i32 %av5 %bv5
  %s3_6 = add i32 %av6 %bv6
  %s3_7 = add i32 %av7 %bv7
  %s4_0 = sub i32 %av0 %bv0
  %s4_1 = sub i32 %av1 %bv1
  %s4_2 = sub i32 %av2 %bv2
  %s4_3 = sub i32 %av3 %bv3
  %s4_4 = sub i32 %av4 %bv4
  %s4_5 = sub i32 %av5 %bv5
  %s4_6 = sub i32 %av6 %bv6
  %s4_7 = sub i32 %av7 %bv7
  %t0_0 = xor i32 %cv0 %dv0
  %t0_1 = xor i32 %cv1 %dv1
  %t0_2 = xor i32 %cv2 %dv2
  %t0_3 = xor i32 %cv3 %dv3
  %t0_4 = xor i32 %cv4 %dv4
  %t0_5 = xor i32 %cv5 %dv5
  %t0_6 = xor i32 %cv6 %dv6
  %t0_7 = xor i32 %cv7 %dv7
  %t1_0 = and i32 %cv0 %dv0
  %t1_1 = and i32 %cv1 %dv1
  %t1_2 = and i32 %cv2 %dv2
  %t1_3 = and i32 %cv3 %dv3
  %t1_4 = and i32 %cv4 %dv4
  %t1_5 = and i32 %cv5 %dv5
  %t1_6 = and i32 %cv6 %dv6
  %t1_7 = and i32 %cv7 %dv7
  %t2_0 = or i32 %cv0 %dv0
  %t2_1 = or i32 %cv1 %dv1
  %t2_2 = or i32 %cv2 %dv2
  %t2_3 = or i32 %cv3 %dv3
  %t2_4 = or i32 %cv4 %dv4
  %t2_5 = or i32 %cv5 %dv5
  %t2_6 = or i32 %cv6 %dv6
  %t2_7 = or i32 %cv7 %dv7
  %t3_0 = add i32 %cv0 %dv0
  %t3_1 = add i32 %cv1 %dv1
  %t3_2 = add i32 %cv2 %dv2
  %t3_3 = add i32 %cv3 %dv3
  %t3_4 = add i32 %cv4 %dv4
  %t3_5 = add i32 %cv5 %dv5
  %t3_6 = add i32 %cv6 %dv6
  %t3_7 = add i32 %cv7 %dv7
  %t4_0 = sub i32 %cv0 %dv0
  %t4_1 = sub i32 %cv1 %dv1
  %t4_2 = sub i32 %cv2 %dv2
  %t4_3 = sub i32 %cv3 %dv3
  %t4_4 = sub i32 %cv4 %dv4
  %t4_5 = sub i32 %cv5 %dv5
  %t4_6 = sub i32 %cv6 %dv6
  %t4_7 = sub i32 %cv7 %dv7
  %p0_0 = add i32 %s0_0 %t0_0
  %p0_1 = add i32 %s0_1 %t0_1
  %p0_2 = add i32 %s0_2 %t0_2
  %p0_3 = add i32 %s0_3 %t0_3
  %p0_4 = add i32 %s0_4 %t0_4
  %p0_5 = add i32 %s0_5 %t0_5
  %p0_6 = add i32 %s0_6 %t0_6
  %p0_7 = add i32 %s0_7 %t0_7
  %p1_0 = add i32 %s1_0 %t1_0
  %p1_1 = add i32 %s1_1 %t1_1
  %p1_2 = add i32 %s1_2 %t1_2
  %p1_3 = add i32 %s1_3 %t1_3
  %p1_4 = add i32 %s1_4 %t1_4
  %p1_5 = add i32 %s1_5 %t1_5
  %p1_6 = add i32 %s1_6 %t1_6
  %p1_7 = add i32 %s1_7 %t1_7
  %p2_0 = add i32 %s2_0 %t2_0
  %p2_1 = add i32 %s2_1 %t2_1
  %p2_2 = add i32 %s2_2 %t2_2
  %p2_3 = add i32 %s2_3 %t2_3
  %p2_4 = add i32 %s2_4 %t2_4
  %p2_5 = add i32 %s2_5 %t2_5
  %p2_6 = add i32 %s2_6 %t2_6
  %p2_7 = add i32 %s2_7 %t2_7
  %p3_0 = add i32 %s3_0 %t3_0
  %p3_1 = add i32 %s3_1 %t3_1
  %p3_2 = add i32 %s3_2 %t3_2
  %p3_3 = add i32 %s3_3 %t3_3
  %p3_4 = add i32 %s3_4 %t3_4
  %p3_5 = add i32 %s3_5 %t3_5
  %p3_6 = add i32 %s3_6 %t3_6
  %p3_7 = add i32 %s3_7 %t3_7
  %p4_0 = add i32 %s4_0 %t4_0
  %p4_1 = add i32 %s4_1 %t4_1
  %p4_2 = add i32 %s4_2 %t4_2
  %p4_3 = add i32 %s4_3 %t4_3
  %p4_4 = add i32 %s4_4 %t4_4
  %p4_5 = add i32 %s4_5 %t4_5
  %p4_6 = add i32 %s4_6 %t4_6
  %p4_7 = add i32 %s4_7 %t4_7
  %q0_0 = xor i32 %s0_0 %t1_0
  %q0_1 = xor i32 %s0_1 %t1_1
  %q0_2 = xor i32 %s0_2 %t1_2
  %q0_3 = xor i32 %s0_3 %t1_3
  %q0_4 = xor i32 %s0_4 %t1_4
  %q0_5 = xor i32 %s0_5 %t1_5
  %q0_6 = xor i32 %s0_6 %t1_6
  %q0_7 = xor i32 %s0_7 %t1_7
  %q1_0 = xor i32 %s1_0 %t2_0
  %q1_1 = xor i32 %s1_1 %t2_1
  %q1_2 = xor i32 %s1_2 %t2_2
  %q1_3 = xor i32 %s1_3 %t2_3
  %q1_4 = xor i32 %s1_4 %t2_4
  %q1_5 = xor i32 %s1_5 %t2_5
  %q1_6 = xor i32 %s1_6 %t2_6
  %q1_7 = xor i32 %s1_7 %t2_7
  %q2_0 = xor i32 %s2_0 %t3_0
  %q2_1 = xor i32 %s2_1 %t3_1
  %q2_2 = xor i32 %s2_2 %t3_2
  %q2_3 = xor i32 %s2_3 %t3_3
  %q2_4 = xor i32 %s2_4 %t3_4
  %q2_5 = xor i32 %s2_5 %t3_5
  %q2_6 = xor i32 %s2_6 %t3_6
  %q2_7 = xor i32 %s2_7 %t3_7
  %q3_0 = xor i32 %s3_0 %t4_0
  %q3_1 = xor i32 %s3_1 %t4_1
  %q3_2 = xor i32 %s3_2 %t4_2
  %q3_3 = xor i32 %s3_3 %t4_3
  %q3_4 = xor i32 %s3_4 %t4_4
  %q3_5 = xor i32 %s3_5 %t4_5
  %q3_6 = xor i32 %s3_6 %t4_6
  %q3_7 = xor i32 %s3_7 %t4_7
  %q4_0 = xor i32 %s4_0 %t0_0
  %q4_1 = xor i32 %s4_1 %t0_1
  %q4_2 = xor i32 %s4_2 %t0_2
  %q4_3 = xor i32 %s4_3 %t0_3
  %q4_4 = xor i32 %s4_4 %t0_4
  %q4_5 = xor i32 %s4_5 %t0_5
  %q4_6 = xor i32 %s4_6 %t0_6
  %q4_7 = xor i32 %s4_7 %t0_7
  %pa0_0 = add i32 %p0_0 %p1_0
  %pa1_0 = add i32 %pa0_0 %p2_0
  %pa2_0 = add i32 %pa1_0 %p3_0
  %psum_0 = add i32 %pa2_0 %p4_0
  %qa0_0 = add i32 %q0_0 %q1_0
  %qa1_0 = add i32 %qa0_0 %q2_0
  %qa2_0 = add i32 %qa1_0 %q3_0
  %qsum_0 = add i32 %qa2_0 %q4_0
  %mix_0 = xor i32 %psum_0 %qsum_0
  %v0 = add i32 %mix_0 %ev0
  %pa0_1 = add i32 %p0_1 %p1_1
  %pa1_1 = add i32 %pa0_1 %p2_1
  %pa2_1 = add i32 %pa1_1 %p3_1
  %psum_1 = add i32 %pa2_1 %p4_1
  %qa0_1 = add i32 %q0_1 %q1_1
  %qa1_1 = add i32 %qa0_1 %q2_1
  %qa2_1 = add i32 %qa1_1 %q3_1
  %qsum_1 = add i32 %qa2_1 %q4_1
  %mix_1 = xor i32 %psum_1 %qsum_1
  %v1 = add i32 %mix_1 %ev1
  %pa0_2 = add i32 %p0_2 %p1_2
  %pa1_2 = add i32 %pa0_2 %p2_2
  %pa2_2 = add i32 %pa1_2 %p3_2
  %psum_2 = add i32 %pa2_2 %p4_2
  %qa0_2 = add i32 %q0_2 %q1_2
  %qa1_2 = add i32 %qa0_2 %q2_2
  %qa2_2 = add i32 %qa1_2 %q3_2
  %qsum_2 = add i32 %qa2_2 %q4_2
  %mix_2 = xor i32 %psum_2 %qsum_2
  %v2 = add i32 %mix_2 %ev2
  %pa0_3 = add i32 %p0_3 %p1_3
  %pa1_3 = add i32 %pa0_3 %p2_3
  %pa2_3 = add i32 %pa1_3 %p3_3
  %psum_3 = add i32 %pa2_3 %p4_3
  %qa0_3 = add i32 %q0_3 %q1_3
  %qa1_3 = add i32 %qa0_3 %q2_3
  %qa2_3 = add i32 %qa1_3 %q3_3
  %qsum_3 = add i32 %qa2_3 %q4_3
  %mix_3 = xor i32 %psum_3 %qsum_3
  %v3 = add i32 %mix_3 %ev3
  %pa0_4 = add i32 %p0_4 %p1_4
  %pa1_4 = add i32 %pa0_4 %p2_4
  %pa2_4 = add i32 %pa1_4 %p3_4
  %psum_4 = add i32 %pa2_4 %p4_4
  %qa0_4 = add i32 %q0_4 %q1_4
  %qa1_4 = add i32 %qa0_4 %q2_4
  %qa2_4 = add i32 %qa1_4 %q3_4
  %qsum_4 = add i32 %qa2_4 %q4_4
  %mix_4 = xor i32 %psum_4 %qsum_4
  %v4 = add i32 %mix_4 %ev4
  %pa0_5 = add i32 %p0_5 %p1_5
  %pa1_5 = add i32 %pa0_5 %p2_5
  %pa2_5 = add i32 %pa1_5 %p3_5
  %psum_5 = add i32 %pa2_5 %p4_5
  %qa0_5 = add i32 %q0_5 %q1_5
  %qa1_5 = add i32 %qa0_5 %q2_5
  %qa2_5 = add i32 %qa1_5 %q3_5
  %qsum_5 = add i32 %qa2_5 %q4_5
  %mix_5 = xor i32 %psum_5 %qsum_5
  %v5 = add i32 %mix_5 %ev5
  %pa0_6 = add i32 %p0_6 %p1_6
  %pa1_6 = add i32 %pa0_6 %p2_6
  %pa2_6 = add i32 %pa1_6 %p3_6
  %psum_6 = add i32 %pa2_6 %p4_6
  %qa0_6 = add i32 %q0_6 %q1_6
  %qa1_6 = add i32 %qa0_6 %q2_6
  %qa2_6 = add i32 %qa1_6 %q3_6
  %qsum_6 = add i32 %qa2_6 %q4_6
  %mix_6 = xor i32 %psum_6 %qsum_6
  %v6 = add i32 %mix_6 %ev6
  %pa0_7 = add i32 %p0_7 %p1_7
  %pa1_7 = add i32 %pa0_7 %p2_7
  %pa2_7 = add i32 %pa1_7 %p3_7
  %psum_7 = add i32 %pa2_7 %p4_7
  %qa0_7 = add i32 %q0_7 %q1_7
  %qa1_7 = add i32 %qa0_7 %q2_7
  %qa2_7 = add i32 %qa1_7 %q3_7
  %qsum_7 = add i32 %qa2_7 %q4_7
  %mix_7 = xor i32 %psum_7 %qsum_7
  %v7 = add i32 %mix_7 %ev7
  store i32 %v0 %dst align 4
  store i32 %v1 %dst1 align 4
  store i32 %v2 %dst2 align 4
  store i32 %v3 %dst3 align 4
  store i32 %v4 %dst4 align 4
  store i32 %v5 %dst5 align 4
  store i32 %v6 %dst6 align 4
  store i32 %v7 %dst7 align 4
  return
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

func @forge_loop_add_i64(%base: ptr, %count: i64, %delta: i64) -> void {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%base, %count)
loop(%p: ptr, %n: i64):
  %done = cmp.eq i64 %n %zero
  branch %done, exit(), body(%p, %n)
body(%q: ptr, %m: i64):
  %value = load i64 %q align 8
  %next_value = add i64 %value %delta
  store i64 %next_value %q align 8
  %next_p = ptr.offset ptr %q 8
  %next_n = sub i64 %m %one
  jump loop(%next_p, %next_n)
exit:
  return
}


}
