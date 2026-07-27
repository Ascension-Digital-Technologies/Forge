module @callback_parameters {
  signature @Unary(%value: i64) -> i64

  func @apply(%callback: callback @Unary, %value: i64) -> i64 {
  entry:
    %result = call.indirect i64 %callback as @Unary(%value)
    return %result
  }
}
