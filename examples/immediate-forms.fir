module @immediate_forms {
func @immediate_forms(%value: i64) -> i64 {
entry:
  %three = const i64 3
  %added = add i64 %value %three
  %five = const i64 5
  %multiplied = mul i64 %added %five
  %two = const i64 2
  %shifted = shl i64 %multiplied %two
  %seven = const i64 7
  %result = sub i64 %shifted %seven
  return %result
}
}
