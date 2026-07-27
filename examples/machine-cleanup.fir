module @machine_cleanup {
func @cleanup(%value: i64) -> i64 {
entry:
  %first = copy i64 %value
  %second = copy i64 %first
  %third = copy i64 %second
  return %third
}
}
