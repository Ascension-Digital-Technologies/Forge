module @aggregate_borrows {
struct @Record { value: i64 }
func @read_borrow(%input: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %input %field align 8
  %borrow = aggregate.borrow.struct ptr %record @Record
  %borrowed_field = struct.field.name.address ptr %borrow @Record value
  %result = load i64 %borrowed_field align 8
  aggregate.borrow.end.struct void %borrow @Record
  aggregate.end.struct void %record @Record
  return %result
}
}
