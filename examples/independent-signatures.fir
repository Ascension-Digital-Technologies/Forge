module @independent_signatures {
  signature @Unary(%value: i64) -> i64

  func @increment(%value: i64) -> i64 {
  entry:
    %one = const i64 1
    %result = add i64 %value %one
    return %result
  }

  func @dispatch(%value: i64) -> i64 {
  entry:
    %target = func.address ptr @increment as @Unary
    %result = call.indirect i64 %target as @Unary(%value)
    return %result
  }
}
