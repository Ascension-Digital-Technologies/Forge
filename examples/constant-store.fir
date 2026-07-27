module @constant_store {
func @store_constant() -> i64 {
entry:
  %slot = stack.alloc ptr 8 align 8
  %value = const i64 42
  store i64 %value %slot align 8
  %loaded = load i64 %slot align 8
  return %loaded
}
}
