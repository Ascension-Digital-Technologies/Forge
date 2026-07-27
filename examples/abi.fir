module @abi_examples {
  func @sum_eight(%a: i64, %b: i64, %c: i64, %d: i64, %e: i64, %f: i64, %g: i64, %h: i64) -> i64 {
  entry:
    %ab = add i64 %a, %b
    %cd = add i64 %c, %d
    %ef = add i64 %e, %f
    %gh = add i64 %g, %h
    %abcd = add i64 %ab, %cd
    %efgh = add i64 %ef, %gh
    %sum = add i64 %abcd, %efgh
    return %sum
  }
}
