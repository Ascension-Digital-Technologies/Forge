module @borrowed_returns {
  struct @Record { value: i64 }

  func @identity(%record: borrow struct @Record) -> borrow struct @Record from 0 {
  entry:
    return %record
  }

  func @read(%value: i64) -> i64 {
  entry:
    %record = stack.alloc.struct ptr @Record
    %field = struct.field.name.address ptr %record @Record value
    store i64 %value %field align 8
    %loan = call ptr @identity(%record)
    %loan_field = struct.field.name.address ptr %loan @Record value
    %result = load i64 %loan_field align 8
    return %result
  }
}
