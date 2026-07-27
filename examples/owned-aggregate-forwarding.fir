module @owned_aggregate_forwarding {
array @Words = i16[8]
struct @Record { value: i64, words: array @Words }

func @make_record(%value: i64) -> owned struct @Record {
entry:
  %record = stack.alloc.struct ptr @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  return %record
}

func @forward_record(%value: i64) -> owned struct @Record {
entry:
  %record = call ptr @make_record(%value)
  return %record
}

func @read_forwarded_record(%value: i64) -> i64 {
entry:
  %record = call ptr @forward_record(%value)
  %field = struct.field.name.address ptr %record @Record value
  %result = load i64 %field align 8
  return %result
}
}
