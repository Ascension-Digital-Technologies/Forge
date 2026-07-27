module @nested_aggregates {
  array @Words = i16[8]
  struct @Inner { value: i64, words: array @Words }
  struct @Outer { tag: i8, inner: struct @Inner, tail: i64 }

  func @read_nested(%value: i64) -> i64 {
  entry:
    %storage = stack.alloc ptr 40 align 8
    %inner = struct.field.name.address ptr %storage @Outer inner
    %words = struct.field.name.address ptr %inner @Inner words
    %element = array.element.address ptr %words @Words 5
    %tail = struct.field.name.address ptr %storage @Outer tail
    %narrow = truncate i16 %value
    store i16 %narrow %element align 2
    store i64 %value %tail align 8
    %loaded = load i16 %element align 2
    %wide = zero_extend i64 %loaded
    %tail_value = load i64 %tail align 8
    %result = xor i64 %wide %tail_value
    return %result
  }
}
