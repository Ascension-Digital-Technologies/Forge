module @compare_branch {
  func @select_less(%left: i32, %right: i32) -> i32 {
  entry:
    %less = cmp.lt i32 %left %right
    branch %less, yes(), no()

  yes:
    return %left

  no:
    return %right
  }
}
