module @aggregate_lifetimes {
struct @Record { value: i64 }

func @read_then_end(%input: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %input %field align 8
  %result = load i64 %field align 8
  aggregate.end.struct void %record @Record
  return %result
}
}
