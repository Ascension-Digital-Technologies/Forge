module @globals {
  global @counter: i64 = 40
  constant @increment: i64 = 2

  func @main() -> i64 {
  entry:
    %counter_address = global.address ptr @counter
    %increment_address = global.address ptr @increment
    %counter_value = load i64 %counter_address
    %increment_value = load i64 %increment_address
    %next = add i64 %counter_value %increment_value
    store i64 %next %counter_address
    return %next
  }
}
