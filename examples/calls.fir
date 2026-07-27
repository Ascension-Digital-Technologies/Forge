module @calls {
  func @factorial(%value: i32) -> i32 {
  entry:
    %one = const i32 1
    %base = cmp.le i32 %value %one
    branch %base, done(%one), recurse(%value)
  recurse(%current: i32):
    %next = sub i32 %current %one
    %partial = call i32 @factorial(%next)
    %product = mul i32 %current %partial
    return %product
  done(%base_result: i32):
    return %base_result
  }
}
