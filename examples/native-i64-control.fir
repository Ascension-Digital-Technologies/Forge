module @wide_control {
  func @sum_to_wide(%limit: i64) -> i64 {
  entry:
    %zero = const i64 0
    %one = const i64 1
    jump loop(%zero, %zero)

  loop(%index: i64, %total: i64):
    %done = cmp.ge i64 %index %limit
    branch %done, exit(%total), body(%index, %total)

  body(%current: i64, %running: i64):
    %next_total = add i64 %running %current
    %next_index = add i64 %current %one
    jump loop(%next_index, %next_total)

  exit(%result: i64):
    return %result
  }
}
