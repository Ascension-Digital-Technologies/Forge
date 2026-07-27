module @conditional_borrowed_returns {
struct @Record { value: i64 }

func @select(%condition: i1, %record: borrow struct @Record) -> borrow struct @Record from 1 {
entry:
  branch %condition, left(%record), right(%record)
left(%left_record: struct @Record):
  jump join(%left_record)
right(%right_record: struct @Record):
  jump join(%right_record)
join(%selected: struct @Record):
  return %selected
}

func @read_selected(%value: i64, %condition: i1) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  %loan = call ptr @select(%condition, %record)
  %selected_field = struct.field.name.address ptr %loan @Record value
  %result = load i64 %selected_field align 8
  aggregate.borrow.end.struct void %loan @Record
  return %result
}
}
