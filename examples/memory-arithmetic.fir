module @memory_arithmetic {
func @add_loaded(%left: i64) -> i64 {
entry:
  %slot = stack.alloc ptr 8 align 8
  %value = const i64 9
  store i64 %value %slot align 8
  %loaded = load i64 %slot align 8
  %sum = add i64 %left %loaded
  return %sum
}
}
