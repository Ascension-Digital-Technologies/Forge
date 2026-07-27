module @address_mode {
func @load_field(%base: ptr) -> i64 {
entry:
  %field = ptr.offset ptr %base 8
  %value = load i64 %field
  return %value
}
}
