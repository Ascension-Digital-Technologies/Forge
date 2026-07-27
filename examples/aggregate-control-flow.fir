module @aggregate_control_flow {
struct @Record { value: i64 }

func @preserve_original(%input: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %input %field align 8
  jump mutate(%record)

mutate(%copy: owned struct @Record):
  %copy_field = struct.field.name.address ptr %copy @Record value
  %replacement = const i64 999
  store i64 %replacement %copy_field align 8
  %original_field = struct.field.name.address ptr %record @Record value
  %result = load i64 %original_field align 8
  return %result
}
}
