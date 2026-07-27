module @floating_compare_quality {
  func @ordered_less(%left: f64, %right: f64) -> i32 {
  entry:
    %condition = cmp.lt f64 %left %right
    branch %condition, yes(), no()
  yes:
    %one = const i32 1
    return %one
  no:
    %zero = const i32 0
    return %zero
  }
}
