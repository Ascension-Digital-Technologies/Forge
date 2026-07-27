module @cfg_cleanup {
  func @threaded() -> i32 {
  entry:
    jump bridge()

  bridge:
    jump exit()

  dead:
    %unused = const i32 99
    return %unused

  exit:
    %value = const i32 7
    return %value
  }
}
