module @moveonly_example {
moveonly struct @Token { value: i64 }

func @consume(%input: i64) -> i64 {
entry:
  %token = stack.alloc.struct ptr @Token
  aggregate.zero.struct void %token @Token
  %field = struct.field.name.address ptr %token @Token value
  store i64 %input %field align 8
  %moved = aggregate.move.struct ptr %token @Token
  %result_address = struct.field.name.address ptr %moved @Token value
  %result = load i64 %result_address align 8
  aggregate.end.struct void %moved @Token
  return %result
}
}
