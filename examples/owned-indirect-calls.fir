module @owned_indirect_calls {
struct @Pair { first: i64, second: i64 }

func @make_pair(%value: i64) -> owned struct @Pair {
entry:
  %pair = stack.alloc.struct ptr @Pair
  aggregate.zero.struct void %pair @Pair
  %first = struct.field.name.address ptr %pair @Pair first
  %second = struct.field.name.address ptr %pair @Pair second
  %seven = const i64 7
  store i64 %value %first align 8
  store i64 %seven %second align 8
  return %pair
}

func @read(%value: i64) -> i64 {
entry:
  %target = func.address ptr @make_pair
  %pair = call.indirect ptr %target as @make_pair(%value)
  %first = struct.field.name.address ptr %pair @Pair first
  %second = struct.field.name.address ptr %pair @Pair second
  %a = load i64 %first align 8
  %b = load i64 %second align 8
  %result = add i64 %a %b
  return %result
}
}
