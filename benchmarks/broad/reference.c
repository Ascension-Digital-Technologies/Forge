// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

__attribute__((noinline)) uint64_t llvm_int_mix(uint64_t seed, int64_t rounds) {
  uint64_t x = seed;
  for (int64_t i = 0; i < rounds; ++i) {
    x = x * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    x ^= x >> 13;
    x ^= x << 7;
  }
  return x;
}
__attribute__((noinline)) uint64_t llvm_fib(int64_t n) {
  uint64_t a=0,b=1;
  for (int64_t i=0;i<n;++i) { uint64_t s=a+b; a=b; b=s; }
  return a;
}
__attribute__((noinline)) uint64_t llvm_branch_walk(uint64_t seed, int64_t rounds) {
  uint64_t x=seed;
  for (int64_t i=0;i<rounds;++i) x=(x&1)?x*3+7:(x>>1)^5;
  return x;
}
__attribute__((noinline)) int64_t llvm_reg_pressure(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f,int64_t g,int64_t h) {
  int64_t ab=a+b, cd=c+d, ef=e+f, gh=g+h;
  int64_t x=(ab*cd)^(ef*gh);
  return x+ab+ef;
}
__attribute__((noinline)) double llvm_float_poly(double x, int64_t rounds) {
  for (int64_t i=0;i<rounds;++i) x=x*1.0000001192092896+0.00000095367431640625;
  return x;
}
__attribute__((noinline)) int64_t llvm_memory4(const int64_t *p) { return p[0]+p[1]+p[2]+p[3]; }

__attribute__((noinline)) int64_t llvm_call_leaf(int64_t x,int64_t y,int64_t z) { return ((x+y)*z)^7; }
__attribute__((noinline)) int64_t llvm_call_chain(int64_t x) {
  int64_t a=llvm_call_leaf(x,2,3); int64_t b=llvm_call_leaf(a,3,2);
  int64_t c=llvm_call_leaf(b,2,3); return llvm_call_leaf(c,3,2);
}
__attribute__((noinline)) double llvm_float_leaf(double x,double y) { return x*y+0.125; }
__attribute__((noinline)) double llvm_float_calls(double x) {
  double r0=llvm_float_leaf(x,1.5); double r1=llvm_float_leaf(r0,0.75); return llvm_float_leaf(r1,1.5);
}
__attribute__((noinline)) int64_t llvm_memory_sum(const int64_t *p) { return p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7]; }
__attribute__((noinline)) int32_t llvm_memory_sum_i32(const int32_t *p) { return p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7]; }
__attribute__((noinline)) void llvm_memory_add4(int64_t *p, int64_t delta) {
  p[0] += delta; p[1] += delta; p[2] += delta; p[3] += delta;
}
__attribute__((noinline)) void llvm_memory_xor4_i32(int32_t *p, int32_t mask) {
  p[0] ^= mask; p[1] ^= mask; p[2] ^= mask; p[3] ^= mask;
}
__attribute__((noinline)) void llvm_memory_and4_i64(int64_t *p, int64_t mask) {
  p[0] &= mask; p[1] &= mask; p[2] &= mask; p[3] &= mask;
}
__attribute__((noinline)) void llvm_memory_xor_copy8_i32(const int32_t *src, int32_t *dst, int32_t mask) {
  dst[0]=src[0]^mask; dst[1]=src[1]^mask; dst[2]=src[2]^mask; dst[3]=src[3]^mask;
  dst[4]=src[4]^mask; dst[5]=src[5]^mask; dst[6]=src[6]^mask; dst[7]=src[7]^mask;
}
__attribute__((noinline)) void llvm_memory_add_copy8_i32(const int32_t *lhs, const int32_t *rhs, int32_t *dst) {
  dst[0]=lhs[0]+rhs[0]; dst[1]=lhs[1]+rhs[1]; dst[2]=lhs[2]+rhs[2]; dst[3]=lhs[3]+rhs[3];
  dst[4]=lhs[4]+rhs[4]; dst[5]=lhs[5]+rhs[5]; dst[6]=lhs[6]+rhs[6]; dst[7]=lhs[7]+rhs[7];
}
__attribute__((noinline)) void llvm_memory_chain_copy8_i32(const int32_t *a, const int32_t *b, const int32_t *c, int32_t *dst) {
  dst[0]=(a[0]^b[0])+c[0]; dst[1]=(a[1]^b[1])+c[1]; dst[2]=(a[2]^b[2])+c[2]; dst[3]=(a[3]^b[3])+c[3];
  dst[4]=(a[4]^b[4])+c[4]; dst[5]=(a[5]^b[5])+c[5]; dst[6]=(a[6]^b[6])+c[6]; dst[7]=(a[7]^b[7])+c[7];
}
__attribute__((noinline)) void llvm_memory_deep_chain_copy8_i32(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, const int32_t *e, int32_t *dst) {
  dst[0]=(((a[0]^b[0])+c[0])&d[0])-e[0]; dst[1]=(((a[1]^b[1])+c[1])&d[1])-e[1];
  dst[2]=(((a[2]^b[2])+c[2])&d[2])-e[2]; dst[3]=(((a[3]^b[3])+c[3])&d[3])-e[3];
  dst[4]=(((a[4]^b[4])+c[4])&d[4])-e[4]; dst[5]=(((a[5]^b[5])+c[5])&d[5])-e[5];
  dst[6]=(((a[6]^b[6])+c[6])&d[6])-e[6]; dst[7]=(((a[7]^b[7])+c[7])&d[7])-e[7];
}
__attribute__((noinline)) void llvm_memory_branch_dag_copy8_i32(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, const int32_t *e, int32_t *dst) {
  dst[0]=((a[0]^b[0])+(c[0]&d[0]))-e[0]; dst[1]=((a[1]^b[1])+(c[1]&d[1]))-e[1];
  dst[2]=((a[2]^b[2])+(c[2]&d[2]))-e[2]; dst[3]=((a[3]^b[3])+(c[3]&d[3]))-e[3];
  dst[4]=((a[4]^b[4])+(c[4]&d[4]))-e[4]; dst[5]=((a[5]^b[5])+(c[5]&d[5]))-e[5];
  dst[6]=((a[6]^b[6])+(c[6]&d[6]))-e[6]; dst[7]=((a[7]^b[7])+(c[7]&d[7]))-e[7];
}

__attribute__((noinline)) void llvm_memory_shared_dag_copy8_i32(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, const int32_t *e, int32_t *dst) {
  { int32_t s=a[0]^b[0]; dst[0]=((s+c[0])+(s&d[0]))^e[0]; }
  { int32_t s=a[1]^b[1]; dst[1]=((s+c[1])+(s&d[1]))^e[1]; }
  { int32_t s=a[2]^b[2]; dst[2]=((s+c[2])+(s&d[2]))^e[2]; }
  { int32_t s=a[3]^b[3]; dst[3]=((s+c[3])+(s&d[3]))^e[3]; }
  { int32_t s=a[4]^b[4]; dst[4]=((s+c[4])+(s&d[4]))^e[4]; }
  { int32_t s=a[5]^b[5]; dst[5]=((s+c[5])+(s&d[5]))^e[5]; }
  { int32_t s=a[6]^b[6]; dst[6]=((s+c[6])+(s&d[6]))^e[6]; }
  { int32_t s=a[7]^b[7]; dst[7]=((s+c[7])+(s&d[7]))^e[7]; }
}

__attribute__((noinline)) void llvm_memory_pressure_dag_copy8_i32(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, const int32_t *e, int32_t *dst) {
  { int32_t s0=a[0]^b[0], s1=a[0]&b[0], s2=a[0]|b[0], s3=a[0]+b[0], s4=a[0]-b[0];
    int32_t t0=c[0]^d[0], t1=c[0]&d[0], t2=c[0]|d[0], t3=c[0]+d[0], t4=c[0]-d[0];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[0]=(ps^qs)+e[0]; }
  { int32_t s0=a[1]^b[1], s1=a[1]&b[1], s2=a[1]|b[1], s3=a[1]+b[1], s4=a[1]-b[1];
    int32_t t0=c[1]^d[1], t1=c[1]&d[1], t2=c[1]|d[1], t3=c[1]+d[1], t4=c[1]-d[1];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[1]=(ps^qs)+e[1]; }
  { int32_t s0=a[2]^b[2], s1=a[2]&b[2], s2=a[2]|b[2], s3=a[2]+b[2], s4=a[2]-b[2];
    int32_t t0=c[2]^d[2], t1=c[2]&d[2], t2=c[2]|d[2], t3=c[2]+d[2], t4=c[2]-d[2];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[2]=(ps^qs)+e[2]; }
  { int32_t s0=a[3]^b[3], s1=a[3]&b[3], s2=a[3]|b[3], s3=a[3]+b[3], s4=a[3]-b[3];
    int32_t t0=c[3]^d[3], t1=c[3]&d[3], t2=c[3]|d[3], t3=c[3]+d[3], t4=c[3]-d[3];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[3]=(ps^qs)+e[3]; }
  { int32_t s0=a[4]^b[4], s1=a[4]&b[4], s2=a[4]|b[4], s3=a[4]+b[4], s4=a[4]-b[4];
    int32_t t0=c[4]^d[4], t1=c[4]&d[4], t2=c[4]|d[4], t3=c[4]+d[4], t4=c[4]-d[4];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[4]=(ps^qs)+e[4]; }
  { int32_t s0=a[5]^b[5], s1=a[5]&b[5], s2=a[5]|b[5], s3=a[5]+b[5], s4=a[5]-b[5];
    int32_t t0=c[5]^d[5], t1=c[5]&d[5], t2=c[5]|d[5], t3=c[5]+d[5], t4=c[5]-d[5];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[5]=(ps^qs)+e[5]; }
  { int32_t s0=a[6]^b[6], s1=a[6]&b[6], s2=a[6]|b[6], s3=a[6]+b[6], s4=a[6]-b[6];
    int32_t t0=c[6]^d[6], t1=c[6]&d[6], t2=c[6]|d[6], t3=c[6]+d[6], t4=c[6]-d[6];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[6]=(ps^qs)+e[6]; }
  { int32_t s0=a[7]^b[7], s1=a[7]&b[7], s2=a[7]|b[7], s3=a[7]+b[7], s4=a[7]-b[7];
    int32_t t0=c[7]^d[7], t1=c[7]&d[7], t2=c[7]|d[7], t3=c[7]+d[7], t4=c[7]-d[7];
    int32_t p0=s0+t0, p1=s1+t1, p2=s2+t2, p3=s3+t3, p4=s4+t4;
    int32_t q0=s0^t1, q1=s1^t2, q2=s2^t3, q3=s3^t4, q4=s4^t0;
    int32_t ps=((p0+p1)+p2)+p3+p4, qs=((q0+q1)+q2)+q3+q4; dst[7]=(ps^qs)+e[7]; }
}

__attribute__((noinline)) int64_t llvm_memory_update4(int64_t *p, int64_t delta) {
  p[0] += delta; p[1] += delta; p[2] += delta; p[3] += delta;
  return p[0] + p[1] + p[2] + p[3];
}
__attribute__((noinline)) int64_t llvm_call_live(int64_t x) {
  int64_t called = llvm_call_leaf(x, 2, 3);
  return called + x;
}
__attribute__((noinline)) uint64_t llvm_multi_recurrence(int64_t n) {
  uint64_t a=1,b=2,c=3,d=1;
  for (int64_t i=0;i<n;++i) { uint64_t next=(a+b)^(c+d); a=b; b=c; c=d; d=next; }
  return a+b+c+d;
}
__attribute__((noinline)) uint64_t llvm_branch_merge(uint64_t seed, int64_t rounds) {
  uint64_t value=seed;
  for (int64_t i=0;i<rounds;++i) value=(value&8)?value+5:value^3;
  return value;
}
__attribute__((noinline)) int64_t llvm_store_overwrite(int64_t *p, int64_t value) {
  *p = value;
  *p = value + 1;
  return *p;
}
__attribute__((noinline)) int64_t llvm_global_store_overwrite(int64_t *p, int64_t value) {
  *p = value;
  goto overwrite;
overwrite:
  *p = value + 1;
  goto exit;
exit:
  return *p;
}
__attribute__((noinline)) double llvm_float_dot4(const double *a,const double *b) {
  return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3];
}

__attribute__((noinline)) void llvm_loop_add_i64(int64_t *p, int64_t n, int64_t delta) {
  for (int64_t i = 0; i < n; ++i) p[i] += delta;
}
