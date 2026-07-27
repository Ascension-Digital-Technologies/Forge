module @dynamic_callback_tables {
signature @Unary(%value: i64) -> i64
global @dispatch_table: callback @Unary[2] align 8 = zero
func @increment(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = add i64 %value %one
  return %result
}
func @decrement(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = sub i64 %value %one
  return %result
}
func @dispatch(%value: i64, %index: i64) -> i64 {
entry:
  %table = global.address ptr @dispatch_table
  %slot0 = callback.element.address ptr %table @Unary 0
  %slot1 = callback.element.address ptr %table @Unary 1
  %increment = func.address ptr @increment as @Unary
  %decrement = func.address ptr @decrement as @Unary
  store ptr %increment %slot0
  store ptr %decrement %slot1
  %slot = callback.element.address ptr %table @Unary %index
  %target = load ptr %slot
  %result = call.indirect i64 %target as @Unary(%value)
  return %result
}
}
