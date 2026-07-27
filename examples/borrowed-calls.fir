module @borrowed_calls {
struct @Record { value: i64 }

func @read(%record: borrow struct @Record) -> i64 {
entry:
  %field = struct.field.name.address ptr %record @Record value
  %result = load i64 %field align 8
  return %result
}

func @write(%record: borrow mut struct @Record, %value: i64) -> i64 {
entry:
  %child = aggregate.borrow.mut.struct ptr %record @Record
  %field = struct.field.name.address ptr %child @Record value
  store i64 %value %field align 8
  aggregate.borrow.end.struct void %child @Record
  %parent_field = struct.field.name.address ptr %record @Record value
  %result = load i64 %parent_field align 8
  return %result
}

func @run(%value: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %written = call i64 @write(%record, %value)
  %read = call i64 @read(%record)
  %result = add i64 %written %read
  return %result
}
}
