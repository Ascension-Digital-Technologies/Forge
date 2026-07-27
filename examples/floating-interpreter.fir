module @floating_interpreter {
  func @compute() -> f64 {
  entry:
    %left = const f64 1.5
    %right = const f64 2.25
    %sum = add f64 %left %right
    %scale = const f64 4.0
    %scaled = mul f64 %sum %scale
    %divisor = const f64 3.0
    %result = div f64 %scaled %divisor
    return %result
  }
}
