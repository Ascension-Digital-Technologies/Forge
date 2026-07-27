module @borrow_provenance {
struct @Record { value: i64 }

func @update(%record: struct @Record, %replacement: i64) -> i64 {
entry:
  %borrow = aggregate.borrow.mut.struct ptr %record @Record
  %field = struct.field.name.address ptr %borrow @Record value
  store i64 %replacement %field align 8
  %result = load i64 %field align 8
  aggregate.borrow.end.struct void %borrow @Record
  return %result
}
}
