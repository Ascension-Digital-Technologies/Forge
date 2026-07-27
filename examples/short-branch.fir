module @short_branch {
  func @spin() -> void {
  entry:
    jump loop()

  loop:
    jump loop()
  }
}
