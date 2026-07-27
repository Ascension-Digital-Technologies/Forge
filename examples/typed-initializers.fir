module @typed_initializers {
  array @Words = i16[4]
  struct @Inner { value: i64, words: array @Words }
  struct @Outer { tag: i8, inner: struct @Inner, tail: i16 }

  constant @defaults: array @Words = [10, 20, 30, 40]
  global @state: struct @Outer = {
    tag: 1,
    inner: { value: 42, words: [2, 4, 6, 8] },
    tail: 99
  }

  func @read_default() -> i64 {
  entry:
    %base = global.address ptr @defaults
    %slot = array.element.address ptr %base @Words 2
    %value = load i16 %slot align 2
    %wide = zero_extend i64 %value
    return %wide
  }
}
