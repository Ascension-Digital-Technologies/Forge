module @optimization {
  func @compute(%value: i32) -> i32 {
  entry:
    %zero = const i32 0
    %one = const i32 1
    %always = const i1 1
    %left = add i32 %value %zero
    %right = mul i32 %left %one
    %first = add i32 %right %one
    %duplicate = add i32 %right %one
    branch %always, live(%first, %duplicate), dead(%zero)
  live(%a: i32, %b: i32):
    %answer = add i32 %a %b
    return %answer
  dead(%unused: i32):
    %noise = mul i32 %unused %unused
    return %noise
  }
}
