module @aggregate_moves {
struct @Record { value: i64 }

func @move_through_loop(%input: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %input %field align 8
  %moved = aggregate.move.struct ptr %record @Record
  %zero = const i64 0
  jump loop(%moved, %zero)
loop(%state: struct @Record, %index: i64):
  %one = const i64 1
  %continue = cmp.lt i64 %index %one
  branch %continue, body(%state, %index), exit(%state)
body(%body_state: struct @Record, %body_index: i64):
  %next = add i64 %body_index %one
  jump loop(%body_state, %next)
exit(%final_state: struct @Record):
  %final_field = struct.field.name.address ptr %final_state @Record value
  %result = load i64 %final_field align 8
  return %result
}
}
