module @extension_cleanup {
func @roundtrip(%value: i32) -> i32 {
entry:
  %wide = zero_extend i64 %value
  %narrow = truncate i32 %wide
  return %narrow
}
}
