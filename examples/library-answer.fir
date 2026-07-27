module @library_answer {
  func @answer() -> i64 {
  entry:
    %value = const i64 42
    return %value
  }
}
