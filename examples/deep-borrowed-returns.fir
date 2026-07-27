module @deep_borrowed_returns {
struct @Record { value: i64 }

func @identity(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  return %record
}

func @forward(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  %loan = call ptr @identity(%record)
  return %loan
}

func @loop_forward(%record: borrow struct @Record, %count: i64) -> borrow struct @Record from 0 {
entry:
  jump loop(%record, %count)
loop(%loan: struct @Record, %remaining: i64):
  %zero = const i64 0
  %continue = cmp.gt i64 %remaining %zero
  branch %continue, body(%loan, %remaining), done(%loan)
body(%body_loan: struct @Record, %body_remaining: i64):
  %one = const i64 1
  %next = sub i64 %body_remaining %one
  jump loop(%body_loan, %next)
done(%result_loan: struct @Record):
  return %result_loan
}

func @read(%value: i64, %count: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  %forwarded = call ptr @forward(%record)
  aggregate.borrow.end.struct void %forwarded @Record
  %looped = call ptr @loop_forward(%record, %count)
  %result_field = struct.field.name.address ptr %looped @Record value
  %result = load i64 %result_field align 8
  aggregate.borrow.end.struct void %looped @Record
  return %result
}
}
