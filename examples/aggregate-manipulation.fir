module @aggregate_manipulation {
array @Words = i16[8]
struct @Record { tag: i8, value: i64, words: array @Words }

global @source: struct @Record = { tag: 1, value: 42, words: [1, 2, 3, 4, 5, 6, 7, 8] }
global @destination: struct @Record = zero

func @copy_record() -> i64 {
entry:
  %source_address = global.address ptr @source
  %destination_address = global.address ptr @destination
  aggregate.zero.struct void %destination_address @Record
  aggregate.copy.struct void %destination_address %source_address @Record
  %value_address = struct.field.name.address ptr %destination_address @Record value
  %value = load i64 %value_address align 8
  return %value
}

func @stack_record(%value: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  %value_address = struct.field.name.address ptr %record @Record value
  store i64 %value %value_address align 8
  %result = load i64 %value_address align 8
  return %result
}
}
