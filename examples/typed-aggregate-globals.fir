module @typed_aggregate_globals {
array @Words = i16[8]
struct @Inner { value: i64, words: array @Words }
struct @Outer { tag: i8, inner: struct @Inner, tail: i64 }

global @state: struct @Outer = zero
constant @empty_words: array @Words = zero

func @update(%value: i64) -> i64 {
entry:
  %base = global.address ptr @state
  %inner = struct.field.name.address ptr %base @Outer inner
  %words = struct.field.name.address ptr %inner @Inner words
  %slot = array.element.address ptr %words @Words 5
  %tail = struct.field.name.address ptr %base @Outer tail
  %small = truncate i16 %value
  store i16 %small %slot align 2
  store i64 %value %tail align 8
  %loaded = load i16 %slot align 2
  %wide = zero_extend i64 %loaded
  %result = xor i64 %wide %value
  return %result
}
}
