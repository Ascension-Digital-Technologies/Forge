module @owned_parameters {
  struct @Record { value: i64 }
  global @source: struct @Record = zero

  func @mutate_copy(%record: owned struct @Record) -> void {
  entry:
    %field = struct.field.name.address ptr %record @Record value
    %replacement = const i64 999
    store i64 %replacement %field align 8
    return
  }

  func @preserves_caller(%value: i64) -> i64 {
  entry:
    %record = global.address ptr @source
    %field = struct.field.name.address ptr %record @Record value
    store i64 %value %field align 8
    call void @mutate_copy(%record)
    %result = load i64 %field align 8
    return %result
  }
}
