module @aggregate_globals {
  constant @message: i8[8] align 8 = "Forge!\0\0"
  global @scratch: i8[16] align 16 = zero

  func @message_word() -> i64 {
  entry:
    %address = global.address ptr @message
    %value = load i64 %address align 8
    return %value
  }

  func @write_scratch() -> i64 {
  entry:
    %address = global.address ptr @scratch
    %value = const i64 42
    store i64 %value %address align 8
    %loaded = load i64 %address align 8
    return %loaded
  }
}
