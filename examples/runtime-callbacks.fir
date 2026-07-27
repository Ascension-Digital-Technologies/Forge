module @runtime_callbacks {
  signature @Unary(%value: i64) -> i64

  func @dispatch(%value: i64) -> i64 {
  entry:
    %target = callback.address ptr @host_increment as @Unary
    %result = call.indirect i64 %target as @Unary(%value)
    return %result
  }
}
