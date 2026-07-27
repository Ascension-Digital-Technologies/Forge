module @native_i64 {
  global @wide_counter: i64 = 5000000000
  constant @wide_increment: i64 = 7

  func @wide_math(%left: i64, %right: i64) -> i64 {
  entry:
    %sum = add i64 %left %right
    %scale = const i64 3
    %result = mul i64 %sum %scale
    return %result
  }

  func @update_wide() -> i64 {
  entry:
    %counter = global.address ptr @wide_counter
    %increment = global.address ptr @wide_increment
    %left = load i64 %counter
    %right = load i64 %increment
    %sum = add i64 %left %right
    store i64 %sum %counter
    return %sum
  }
}
