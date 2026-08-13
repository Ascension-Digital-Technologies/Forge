// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
extern void forge_loop_add_vector(int64_t*,int64_t,int64_t);
extern void forge_loop_add_scalar(int64_t*,int64_t,int64_t);
extern void llvm_loop_add(int64_t*,int64_t,int64_t);
static uint64_t ns(){struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static volatile uint64_t sink;
typedef void(*fn)(int64_t*,int64_t,int64_t);
static double bench(fn f,int n,int reps){static int64_t a[8192]; for(int i=0;i<n;i++)a[i]=i; uint64_t s=ns(); for(int r=0;r<reps;r++)f(a,n,(r&3)-1); uint64_t e=ns(); sink^=a[0]^a[n-1]; return (double)(e-s)/reps;}
int main(){for(int nidx=0;nidx<4;nidx++){int nsizes[]={31,128,1024,4096}; int n=nsizes[nidx]; int reps=n<100?1000000:n<1000?300000:n<2000?80000:20000; double v=bench(forge_loop_add_vector,n,reps); double s=bench(forge_loop_add_scalar,n,reps); double l=bench(llvm_loop_add,n,reps); printf("%d %.3f %.3f %.3f vec/scalar=%.3fx vec/llvm=%.3f\n",n,v,s,l,s/v,v/l);} }
