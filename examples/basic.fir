module @example {
  func @sum(%limit: i32) -> i32 {
  entry:
    %zero = const i32 0
    %one = const i32 1
    jump loop(%zero, %zero)
  loop(%index: i32, %total: i32):
    %done = const i1 1
    branch %done, exit(%total), body(%index, %total)
  body(%current: i32, %running: i32):
    %next = add i32 %current %one
    jump loop(%next, %running)
  exit(%result: i32):
    return %result
  }
}
