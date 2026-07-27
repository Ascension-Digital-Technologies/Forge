module @callback_globals {
signature @Unary(%value: i64) -> i64
global @dispatch_slot: callback @Unary align 8 = zero
func @increment(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = add i64 %value %one
  return %result
}
func @dispatch(%value: i64) -> i64 {
entry:
  %slot = global.address ptr @dispatch_slot
  %target = func.address ptr @increment as @Unary
  store ptr %target %slot
  %loaded = load ptr %slot
  %result = call.indirect i64 %loaded as @Unary(%value)
  return %result
}
}
