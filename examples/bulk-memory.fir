module @bulk_memory {
global @source: i8[16] align 8 = zero
global @destination: i8[16] align 8 = zero

func @fill_and_copy(%value: i64) -> i64 {
entry:
  %source = global.address ptr @source
  %destination = global.address ptr @destination
  %byte = truncate i8 %value
  %count = const i64 16
  memory.set void %source %byte %count
  memory.copy void %destination %source %count
  %result = load i64 %destination align 8
  return %result
}
}
