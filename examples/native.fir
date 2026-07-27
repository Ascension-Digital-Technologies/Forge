module @native {
  func @maximum(%left: i32, %right: i32) -> i32 {
  entry:
    %greater = cmp.gt i32 %left %right
    branch %greater, selected(%left), selected(%right)
  selected(%value: i32):
    return %value
  }

  func @sum_to(%limit: i32) -> i32 {
  entry:
    %zero = const i32 0
    %one = const i32 1
    jump loop(%zero, %zero)
  loop(%index: i32, %total: i32):
    %keep_going = cmp.lt i32 %index %limit
    branch %keep_going, body(%index, %total), exit(%total)
  body(%current: i32, %running: i32):
    %next_total = add i32 %running %current
    %next_index = add i32 %current %one
    jump loop(%next_index, %next_total)
  exit(%result: i32):
    return %result
  }
}
