module @immediate_control {
func @classify(%value: i64) -> i64 {
entry:
  %limit = const i64 7
  %less = cmp.lt i64 %value %limit
  branch %less, yes(), no()

yes:
  %zero = const i64 0
  return %zero

no:
  %one = const i64 1
  return %one
}
}
