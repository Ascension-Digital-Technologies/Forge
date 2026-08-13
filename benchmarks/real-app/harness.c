// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern uint64_t forge_int_mix(uint64_t,int64_t), forge_branch_walk(uint64_t,int64_t), forge_multi_recurrence(int64_t), forge_branch_merge(uint64_t,int64_t);
extern int64_t forge_memory_sum(const int64_t*), forge_call_chain(int64_t), forge_memory_update4(int64_t*,int64_t);
extern int32_t forge_memory_sum_i32(const int32_t*);
extern void forge_memory_add_copy8_i32(const int32_t*,const int32_t*,int32_t*);
extern void forge_memory_deep_chain_copy8_i32(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
extern void forge_memory_branch_dag_copy8_i32(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
extern void forge_memory_shared_dag_copy8_i32(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
extern double forge_float_dot4(const double*,const double*), forge_float_poly(double,int64_t);
extern void forge_loop_add_i64(int64_t*,int64_t,int64_t);

extern uint64_t llvm_int_mix(uint64_t,int64_t), llvm_branch_walk(uint64_t,int64_t), llvm_multi_recurrence(int64_t), llvm_branch_merge(uint64_t,int64_t);
extern int64_t llvm_memory_sum(const int64_t*), llvm_call_chain(int64_t), llvm_memory_update4(int64_t*,int64_t);
extern int32_t llvm_memory_sum_i32(const int32_t*);
extern void llvm_memory_add_copy8_i32(const int32_t*,const int32_t*,int32_t*);
extern void llvm_memory_deep_chain_copy8_i32(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
extern void llvm_memory_branch_dag_copy8_i32(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
extern void llvm_memory_shared_dag_copy8_i32(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
extern double llvm_float_dot4(const double*,const double*), llvm_float_poly(double,int64_t);
extern void llvm_loop_add_i64(int64_t*,int64_t,int64_t);

typedef struct {
 uint64_t(*int_mix)(uint64_t,int64_t),(*branch_walk)(uint64_t,int64_t),(*multi_recur)(int64_t),(*branch_merge)(uint64_t,int64_t);
 int64_t(*sum64)(const int64_t*),(*call_chain)(int64_t),(*update4)(int64_t*,int64_t);
 int32_t(*sum32)(const int32_t*);
 void(*add8)(const int32_t*,const int32_t*,int32_t*);
 void(*deep8)(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
 void(*branch8)(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
 void(*shared8)(const int32_t*,const int32_t*,const int32_t*,const int32_t*,const int32_t*,int32_t*);
 double(*dot4)(const double*,const double*),(*poly)(double,int64_t);
 void(*loop_add)(int64_t*,int64_t,int64_t);
} api;

static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC_RAW,&t);return(uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static uint64_t mix64(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}
static void init(int64_t*v,int32_t*a,int32_t*b,int32_t*c,int32_t*d,int32_t*e,size_t n){for(size_t i=0;i<n;i++){uint64_t x=mix64(i+17);v[i]=(int64_t)(x&0x7fffffffffffffffULL);a[i]=(int32_t)x;b[i]=(int32_t)(x>>11);c[i]=(int32_t)(x>>23);d[i]=(int32_t)~x;e[i]=(int32_t)(x*33u);}}
static uint64_t pipeline(const api*A,int rounds,int64_t*v,int32_t*a,int32_t*b,int32_t*c,int32_t*d,int32_t*e,size_t n){
 uint64_t h=0x6a09e667f3bcc909ULL; double f=0.5; double x[4]={1.25,2.5,3.75,5.0},y[4]={.5,.25,.125,.0625}; int32_t t0[8],t1[8];
 for(int r=0;r<rounds;r++){
   A->loop_add(v,(int64_t)n,(r&7)-3);
   for(size_t i=0;i<n;i+=8){A->add8(a+i,b+i,t0);A->deep8(t0,b+i,c+i,d+i,e+i,t1);A->branch8(t1,a+i,b+i,d+i,e+i,c+i);A->shared8(c+i,b+i,a+i,d+i,e+i,a+i);}
   for(size_t i=0;i<n;i+=64){h^=(uint64_t)A->sum64(v+i);h+=(uint32_t)A->sum32(c+i);h^=(uint64_t)A->update4(v+i,(r&7)+1);}
   h^=A->int_mix(h^(uint64_t)r,48);h+=A->branch_walk(h|1,32);h^=(uint64_t)A->call_chain((int64_t)h);h+=A->multi_recur(64);h^=A->branch_merge(h,48);
   f+=A->dot4(x,y);f=A->poly(f,16);
 }
 uint64_t fb;memcpy(&fb,&f,8);for(size_t i=0;i<n;i+=257)h^=mix64((uint64_t)v[i]^(uint32_t)a[i]^(uint32_t)c[i]);return h^fb;
}
int main(int argc,char**argv){size_t n=65536;int rounds=32,samples=9;if(argc>1)rounds=atoi(argv[1]);if(argc>2)samples=atoi(argv[2]);size_t z64=n*8,z32=n*4;
 int64_t*base64=aligned_alloc(64,z64),*v=aligned_alloc(64,z64);int32_t *ba=aligned_alloc(64,z32),*bb=aligned_alloc(64,z32),*bc=aligned_alloc(64,z32),*bd=aligned_alloc(64,z32),*be=aligned_alloc(64,z32),*a=aligned_alloc(64,z32),*b=aligned_alloc(64,z32),*c=aligned_alloc(64,z32),*d=aligned_alloc(64,z32),*e=aligned_alloc(64,z32);if(!e)return 2;init(base64,ba,bb,bc,bd,be,n);
 api F={forge_int_mix,forge_branch_walk,forge_multi_recurrence,forge_branch_merge,forge_memory_sum,forge_call_chain,forge_memory_update4,forge_memory_sum_i32,forge_memory_add_copy8_i32,forge_memory_deep_chain_copy8_i32,forge_memory_branch_dag_copy8_i32,forge_memory_shared_dag_copy8_i32,forge_float_dot4,forge_float_poly,forge_loop_add_i64};
 api L={llvm_int_mix,llvm_branch_walk,llvm_multi_recurrence,llvm_branch_merge,llvm_memory_sum,llvm_call_chain,llvm_memory_update4,llvm_memory_sum_i32,llvm_memory_add_copy8_i32,llvm_memory_deep_chain_copy8_i32,llvm_memory_branch_dag_copy8_i32,llvm_memory_shared_dag_copy8_i32,llvm_float_dot4,llvm_float_poly,llvm_loop_add_i64};
 double sf=0,sl=0;uint64_t ref=0;for(int s=0;s<samples;s++)for(int q=0;q<2;q++){const api*A=((s+q)&1)?&F:&L;memcpy(v,base64,z64);memcpy(a,ba,z32);memcpy(b,bb,z32);memcpy(c,bc,z32);memcpy(d,bd,z32);memcpy(e,be,z32);uint64_t t=ns(),got=pipeline(A,rounds,v,a,b,c,d,e,n),dt=ns()-t;if(s==0&&A==&L)ref=got;else if(got!=ref){fprintf(stderr,"mismatch %llx %llx\n",(unsigned long long)got,(unsigned long long)ref);return 3;}if(A==&F)sf+=dt/1e6;else sl+=dt/1e6;}
 printf("checksum %016llx\n",(unsigned long long)ref);printf("forge_avg_ms %.6f\n",sf/samples);printf("llvm_avg_ms %.6f\n",sl/samples);printf("forge_over_llvm %.6f\n",(sf/sl));printf("speedup_if_forge_wins %.6f\n",sl/sf);return 0;}
