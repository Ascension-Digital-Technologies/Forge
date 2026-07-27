module @aggregate_returns {
  struct @Record { tag: i8, value: i64 }
  global @record: struct @Record = { tag: 1, value: 42 }

  func @borrow_record() -> struct @Record {
  entry:
    %record = global.address ptr @record
    return %record
  }

  func @read_record() -> i64 {
  entry:
    %record = call ptr @borrow_record()
    %value_address = struct.field.name.address ptr %record @Record value
    %value = load i64 %value_address align 8
    return %value
  }
}
