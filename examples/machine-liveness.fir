module @machine_liveness {
  func @live_across_edge(%input: i32) -> i32 {
  entry:
    %dead_constant = const i32 99
    %dead_add = add i32 %input %dead_constant
    %dead_compare = cmp.eq i32 %input %dead_constant
    %one = const i32 1
    %live = add i32 %input %one
    jump exit(%live)

  exit(%value: i32):
    return %value
  }
}
