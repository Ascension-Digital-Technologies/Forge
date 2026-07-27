module @interpreter_demo {
  func @factorial(%value: i64) -> i64 {
  entry:
    %one = const i64 1
    %base = cmp.le i64 %value %one
    branch %base, done(%one), recurse(%value)
  recurse(%current: i64):
    %next = sub i64 %current %one
    %partial = call i64 @factorial(%next)
    %product = mul i64 %current %partial
    return %product
  done(%answer: i64):
    return %answer
  }
}
