// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint64_t (*u64_1_fn)(int64_t);
typedef uint64_t (*u64_2_fn)(uint64_t, int64_t);
typedef int64_t (*i64_8_fn)(int64_t, int64_t, int64_t, int64_t,
                            int64_t, int64_t, int64_t, int64_t);
typedef double (*f64_2_fn)(double, int64_t);
typedef int64_t (*mem_fn)(const int64_t*);
typedef int64_t (*call_fn)(int64_t);
typedef double (*float_call_fn)(double);
typedef int64_t (*mem_sum_fn)(const int64_t*);
typedef int64_t (*mem_update_fn)(int64_t*, int64_t);
typedef double (*dot4_fn)(const double*, const double*);

extern uint64_t forge_int_mix(uint64_t, int64_t), llvm_int_mix(uint64_t, int64_t);
extern uint64_t forge_fib(int64_t), llvm_fib(int64_t);
extern uint64_t forge_branch_walk(uint64_t, int64_t), llvm_branch_walk(uint64_t, int64_t);
extern int64_t forge_reg_pressure(int64_t, int64_t, int64_t, int64_t,
                                  int64_t, int64_t, int64_t, int64_t);
extern int64_t llvm_reg_pressure(int64_t, int64_t, int64_t, int64_t,
                                 int64_t, int64_t, int64_t, int64_t);
extern double forge_float_poly(double, int64_t), llvm_float_poly(double, int64_t);
extern int64_t forge_memory4(const int64_t*), llvm_memory4(const int64_t*);
extern int64_t forge_call_chain(int64_t), llvm_call_chain(int64_t);
extern double forge_float_calls(double), llvm_float_calls(double);
extern int64_t forge_memory_sum(const int64_t*), llvm_memory_sum(const int64_t*);
extern int64_t forge_memory_update4(int64_t*, int64_t), llvm_memory_update4(int64_t*, int64_t);
extern int64_t forge_call_live(int64_t), llvm_call_live(int64_t);
extern uint64_t forge_multi_recurrence(int64_t), llvm_multi_recurrence(int64_t);
extern uint64_t forge_branch_merge(uint64_t, int64_t), llvm_branch_merge(uint64_t, int64_t);
extern double forge_float_dot4(const double*, const double*), llvm_float_dot4(const double*, const double*);
extern int64_t forge_store_overwrite(int64_t*, int64_t), llvm_store_overwrite(int64_t*, int64_t);
extern int64_t forge_global_store_overwrite(int64_t*, int64_t), llvm_global_store_overwrite(int64_t*, int64_t);

static volatile uint64_t sink_u64;
static volatile double sink_f64;

enum { dataset_count = 64 };

static uint64_t ns(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * UINT64_C(1000000000) + (uint64_t)time.tv_nsec;
}

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

static double bench_u64_2(u64_2_fn function, uint64_t seed, int64_t rounds, int reps) {
    const uint64_t start = ns();
    uint64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(seed + (uint64_t)i, rounds);
    sink_u64 = sink;
    return (double)(ns() - start) / reps;
}

static double bench_u64_1(u64_1_fn function, int64_t n, int reps) {
    const uint64_t start = ns();
    uint64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(n + (i & 1));
    sink_u64 = sink;
    return (double)(ns() - start) / reps;
}

static double bench_i64_8(i64_8_fn function, int reps) {
    const uint64_t start = ns();
    int64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(i + 1, 2, 3, 4, 5, 6, 7, 8);
    sink_u64 = (uint64_t)sink;
    return (double)(ns() - start) / reps;
}

static double bench_f64(f64_2_fn function, int64_t rounds, int reps) {
    const uint64_t start = ns();
    double sink = 0.0;
    for (int i = 0; i < reps; ++i)
        sink += function(1.25 + (double)(i & 7) * 0.01, rounds);
    sink_f64 = sink;
    return (double)(ns() - start) / reps;
}

static double bench_call(call_fn function, int reps) {
    const uint64_t start = ns();
    int64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(i + 1);
    sink_u64 = (uint64_t)sink;
    return (double)(ns() - start) / reps;
}

static double bench_float_call(float_call_fn function, int reps) {
    const uint64_t start = ns();
    double sink = 0.0;
    for (int i = 0; i < reps; ++i)
        sink += function(1.0 + (double)(i & 7) * 0.01);
    sink_f64 = sink;
    return (double)(ns() - start) / reps;
}

/*
 * Read-only memory kernels deliberately use a preinitialized ring of inputs.
 * The previous harness modified an element immediately before each call. That
 * unfairly penalized a compiler that vectorized the subsequent load because a
 * narrow store followed by a wider overlapping load can trigger a CPU
 * store-forwarding stall. The benchmark should measure the generated load/add
 * sequence, not an artifact of the harness's preceding store width.
 */
static double bench_mem(mem_fn function, int reps) {
    int64_t data[dataset_count][4];
    for (int set = 0; set < dataset_count; ++set)
        for (int item = 0; item < 4; ++item)
            data[set][item] = (int64_t)(set * 17 + item * 3 + 1);

    const uint64_t start = ns();
    int64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(data[i & (dataset_count - 1)]);
    sink_u64 = (uint64_t)sink;
    return (double)(ns() - start) / reps;
}

static double bench_mem_sum(mem_sum_fn function, int reps) {
    int64_t data[dataset_count][8];
    for (int set = 0; set < dataset_count; ++set)
        for (int item = 0; item < 8; ++item)
            data[set][item] = (int64_t)(set * 19 + item * 5 + 1);

    const uint64_t start = ns();
    int64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(data[i & (dataset_count - 1)]);
    sink_u64 = (uint64_t)sink;
    return (double)(ns() - start) / reps;
}

static double bench_mem_update(mem_update_fn function, int reps) {
    int64_t data[4] = {1, 2, 3, 4};
    const uint64_t start = ns();
    int64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(data, (i & 3) - 1);
    sink_u64 = (uint64_t)sink;
    return (double)(ns() - start) / reps;
}

static double bench_dot4(dot4_fn function, int reps) {
    double left[dataset_count][4];
    double right[dataset_count][4];
    for (int set = 0; set < dataset_count; ++set) {
        for (int item = 0; item < 4; ++item) {
            left[set][item] = 1.0 + (double)set * 0.001 + (double)item;
            right[set][item] = 0.5 + (double)set * 0.002 + (double)item;
        }
    }

    const uint64_t start = ns();
    double sink = 0.0;
    for (int i = 0; i < reps; ++i) {
        const int set = i & (dataset_count - 1);
        sink += function(left[set], right[set]);
    }
    sink_f64 = sink;
    return (double)(ns() - start) / reps;
}

static double bench_store_overwrite(mem_update_fn function, int reps) {
    int64_t value = 0;
    const uint64_t start = ns();
    int64_t sink = 0;
    for (int i = 0; i < reps; ++i) sink ^= function(&value, i);
    sink_u64 = (uint64_t)(sink ^ value);
    return (double)(ns() - start) / reps;
}

static void row(const char* name, double forge_ns, double llvm_ns) {
    printf("%-22s %10.3f %10.3f %8.3f\n", name, forge_ns, llvm_ns, forge_ns / llvm_ns);
}

static int check_equivalence(void) {
    uint64_t state = UINT64_C(0x4f726765466f7267);
    for (int sample = 0; sample < 256; ++sample) {
        const uint64_t random = splitmix64(&state);
        const uint64_t seed = random ^ (random >> 17);
        const int64_t rounds = (int64_t)(random & 63U);
        const int64_t n = (int64_t)(random & 127U);
        const int64_t x = (int64_t)(random % 2001U) - 1000;

        if (forge_int_mix(seed, rounds) != llvm_int_mix(seed, rounds)) {
            fprintf(stderr, "int_mix mismatch at sample %d\n", sample);
            return 2;
        }
        if (forge_fib(n) != llvm_fib(n)) {
            fprintf(stderr, "fib mismatch at sample %d\n", sample);
            return 3;
        }
        if (forge_branch_walk(seed, rounds) != llvm_branch_walk(seed, rounds)) {
            fprintf(stderr, "branch_walk mismatch at sample %d\n", sample);
            return 4;
        }
        if (forge_reg_pressure(x, 2, 3, 4, 5, 6, 7, 8) !=
            llvm_reg_pressure(x, 2, 3, 4, 5, 6, 7, 8)) {
            fprintf(stderr, "reg_pressure mismatch at sample %d\n", sample);
            return 5;
        }

        const double input = 0.5 + (double)(random & 255U) / 128.0;
        const double forge_float = forge_float_poly(input, rounds);
        const double llvm_float = llvm_float_poly(input, rounds);
        if (fabs(forge_float - llvm_float) > 1e-9) {
            fprintf(stderr, "float_poly mismatch at sample %d\n", sample);
            return 6;
        }

        int64_t mem4_a[4], mem4_b[4];
        int64_t mem8_a[8], mem8_b[8];
        for (int i = 0; i < 8; ++i) {
            const int64_t value = (int64_t)((splitmix64(&state) % 20001U) - 10000U);
            mem8_a[i] = mem8_b[i] = value;
            if (i < 4) mem4_a[i] = mem4_b[i] = value;
        }
        if (forge_memory4(mem4_a) != llvm_memory4(mem4_b)) {
            fprintf(stderr, "memory4 mismatch at sample %d\n", sample);
            return 7;
        }
        if (forge_memory_sum(mem8_a) != llvm_memory_sum(mem8_b)) {
            fprintf(stderr, "memory_sum mismatch at sample %d\n", sample);
            return 8;
        }

        const int64_t delta = (int64_t)(random % 17U) - 8;
        if (forge_memory_update4(mem4_a, delta) != llvm_memory_update4(mem4_b, delta) ||
            memcmp(mem4_a, mem4_b, sizeof(mem4_a)) != 0) {
            fprintf(stderr, "memory_update4 mismatch at sample %d\n", sample);
            return 9;
        }

        if (forge_call_chain(x) != llvm_call_chain(x) ||
            forge_call_live(x) != llvm_call_live(x)) {
            fprintf(stderr, "integer call mismatch at sample %d\n", sample);
            return 10;
        }
        if (fabs(forge_float_calls(input) - llvm_float_calls(input)) > 1e-12) {
            fprintf(stderr, "float call mismatch at sample %d\n", sample);
            return 11;
        }
        if (forge_multi_recurrence(n) != llvm_multi_recurrence(n)) {
            fprintf(stderr, "multi_recurrence mismatch at sample %d\n", sample);
            return 12;
        }
        if (forge_branch_merge(seed, rounds) != llvm_branch_merge(seed, rounds)) {
            fprintf(stderr, "branch_merge mismatch at sample %d\n", sample);
            return 13;
        }

        double dot_a[4], dot_b[4];
        for (int i = 0; i < 4; ++i) {
            dot_a[i] = (double)((splitmix64(&state) & 1023U) + 1U) / 64.0;
            dot_b[i] = (double)((splitmix64(&state) & 1023U) + 1U) / 128.0;
        }
        if (fabs(forge_float_dot4(dot_a, dot_b) - llvm_float_dot4(dot_a, dot_b)) > 1e-10) {
            fprintf(stderr, "float_dot4 mismatch at sample %d\n", sample);
            return 14;
        }

        int64_t store_a = 0, store_b = 0;
        if (forge_store_overwrite(&store_a, x) != llvm_store_overwrite(&store_b, x) ||
            store_a != store_b) {
            fprintf(stderr, "store_overwrite mismatch at sample %d\n", sample);
            return 15;
        }
        store_a = store_b = 0;
        if (forge_global_store_overwrite(&store_a, x) != llvm_global_store_overwrite(&store_b, x) ||
            store_a != store_b) {
            fprintf(stderr, "global_store_overwrite mismatch at sample %d\n", sample);
            return 16;
        }
    }
    return 0;
}

#define BENCH_ROW(name, forge_expression, llvm_expression) \
    do { \
        double forge_time; \
        double llvm_time; \
        if (reverse_order) { \
            llvm_time = (llvm_expression); \
            forge_time = (forge_expression); \
        } else { \
            forge_time = (forge_expression); \
            llvm_time = (llvm_expression); \
        } \
        row((name), forge_time, llvm_time); \
    } while (0)

int main(void) {
    const int equivalence = check_equivalence();
    if (equivalence != 0) return equivalence;

    const int reverse_order = getenv("FORGE_BENCH_REVERSE") != NULL;
    printf("kernel                  forge_ns    llvm_ns    ratio\n");
    BENCH_ROW("int_mix_1000", bench_u64_2(forge_int_mix, 7, 1000, 50000),
              bench_u64_2(llvm_int_mix, 7, 1000, 50000));
    BENCH_ROW("fib_100", bench_u64_1(forge_fib, 100, 1000000),
              bench_u64_1(llvm_fib, 100, 1000000));
    BENCH_ROW("branch_walk_200", bench_u64_2(forge_branch_walk, 17, 200, 200000),
              bench_u64_2(llvm_branch_walk, 17, 200, 200000));
    BENCH_ROW("reg_pressure", bench_i64_8(forge_reg_pressure, 3000000),
              bench_i64_8(llvm_reg_pressure, 3000000));
    BENCH_ROW("float_poly_200", bench_f64(forge_float_poly, 200, 200000),
              bench_f64(llvm_float_poly, 200, 200000));
    BENCH_ROW("memory4", bench_mem(forge_memory4, 5000000),
              bench_mem(llvm_memory4, 5000000));
    BENCH_ROW("call_chain", bench_call(forge_call_chain, 1500000),
              bench_call(llvm_call_chain, 1500000));
    BENCH_ROW("float_calls", bench_float_call(forge_float_calls, 1500000),
              bench_float_call(llvm_float_calls, 1500000));
    BENCH_ROW("memory_sum_8", bench_mem_sum(forge_memory_sum, 150000),
              bench_mem_sum(llvm_memory_sum, 150000));
    BENCH_ROW("memory_update4", bench_mem_update(forge_memory_update4, 1000000),
              bench_mem_update(llvm_memory_update4, 1000000));
    BENCH_ROW("call_live", bench_call(forge_call_live, 1500000),
              bench_call(llvm_call_live, 1500000));
    BENCH_ROW("multi_recur_100", bench_u64_1(forge_multi_recurrence, 100, 500000),
              bench_u64_1(llvm_multi_recurrence, 100, 500000));
    BENCH_ROW("branch_merge_200", bench_u64_2(forge_branch_merge, 17, 200, 200000),
              bench_u64_2(llvm_branch_merge, 17, 200, 200000));
    BENCH_ROW("float_dot4", bench_dot4(forge_float_dot4, 2000000),
              bench_dot4(llvm_float_dot4, 2000000));
    BENCH_ROW("store_overwrite", bench_store_overwrite(forge_store_overwrite, 3000000),
              bench_store_overwrite(llvm_store_overwrite, 3000000));
    BENCH_ROW("global_store_overwrite", bench_store_overwrite(forge_global_store_overwrite, 3000000),
              bench_store_overwrite(llvm_global_store_overwrite, 3000000));
    return 0;
}
