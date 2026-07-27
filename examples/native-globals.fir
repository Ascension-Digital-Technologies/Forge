module @native_globals {
  global @counter: i32 = 40
  constant @increment: i32 = 2

  func @update_counter() -> i32 {
  entry:
    %counter = global.address ptr @counter
    %increment = global.address ptr @increment
    %left = load i32 %counter
    %right = load i32 %increment
    %sum = add i32 %left %right
    store i32 %sum %counter
    return %sum
  }

  func @read_counter() -> i32 {
  entry:
    %counter = global.address ptr @counter
    %value = load i32 %counter
    return %value
  }
}
