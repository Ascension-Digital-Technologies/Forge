module @abi_hardening {
  extern func @mixed_weighted(%i0: i64, %a: f64, %i1: i64, %b: f64, %c: f64, %d: f64) -> f64
  func @abi_stress(%i0: i64, %a: f64, %i1: i64, %b: f64, %c: f64, %d: f64) -> f64 {
  entry:
    %result = call f64 @mixed_weighted(%i1, %d, %i0, %c, %b, %a)
    return %result
  }
}
