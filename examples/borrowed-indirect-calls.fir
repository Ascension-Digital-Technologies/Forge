module @borrowed_indirect_calls {
struct @Record { value: i64 }
func @identity(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  return %record
}
func @read(%value: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  %target = func.address ptr @identity
  %loan = call.indirect ptr %target as @identity(%record)
  %loan_field = struct.field.name.address ptr %loan @Record value
  %result = load i64 %loan_field align 8
  aggregate.borrow.end.struct void %loan @Record
  return %result
}
}
