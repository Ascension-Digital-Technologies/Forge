#!/usr/bin/env python3
# Copyright 2026 Mario Vinciguerra
# SPDX-License-Identifier: Apache-2.0
import argparse, pathlib, subprocess, statistics, re, json
root = pathlib.Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', default='build')
p.add_argument('--target', choices=['sse2','sse41','avx','avx2','avx512'], default='avx2')
p.add_argument('--samples', type=int, default=5)
p.add_argument('--cc', default='clang')
p.add_argument('--forge-opt', choices=['O0','O1','O2','O3'], default='O3')
p.add_argument('--llvm-opt', choices=['O0','O1','O2','O3'], default='O3')
a = p.parse_args()
out = root/'build'/'loop-vector-bench'; out.mkdir(parents=True, exist_ok=True)
forge = root/a.build/'forge'
def run(cmd): subprocess.run([str(x) for x in cmd], cwd=root, check=True)
target_flags={'sse2':['-msse2'],'sse41':['-msse4.1'],'avx':['-mavx'],'avx2':['-mavx2'],'avx512':['-mavx512f','-mavx512bw','-mavx512vl']}[a.target]
run([forge, 'compile', root/'benchmarks/loop-vector/kernels.fir', '-'+a.forge_opt, '--format=elf', f'--x86-vector={a.target}', '-o', out/'forge.o'])
run([a.cc, '-'+a.llvm_opt, *target_flags, '-c', root/'benchmarks/loop-vector/reference.c', '-o', out/'llvm.o'])
run([a.cc, '-O3', *target_flags, root/'benchmarks/loop-vector/harness.c', out/'forge.o', out/'llvm.o', '-o', out/'loop-vector-bench'])
rows = {}
pattern = re.compile(r'^(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+vec/scalar=([0-9.]+)x\s+vec/llvm=([0-9.]+)$')
for _ in range(a.samples):
    text = subprocess.check_output([out/'loop-vector-bench'], cwd=root, text=True)
    for line in text.splitlines():
        m = pattern.match(line.strip())
        if not m: continue
        n = int(m.group(1)); vals = tuple(float(m.group(i)) for i in range(2,7))
        rows.setdefault(n, []).append(vals)
print(f'target: {a.target}')
print('elements  vector_ns  scalar_ns  llvm_ns  scalar/vector  vector/llvm')
report = {}
for n in sorted(rows):
    v = rows[n]
    med = [statistics.median(x[i] for x in v) for i in range(5)]
    report[str(n)] = {'vector_ns':med[0],'scalar_ns':med[1],'llvm_ns':med[2],
                      'scalar_over_vector':med[3],'vector_over_llvm':med[4]}
    print(f'{n:8d} {med[0]:10.3f} {med[1]:10.3f} {med[2]:8.3f} {med[3]:13.3f}x {med[4]:11.3f}x')
report['_metadata']={'target':a.target,'samples':a.samples,'forge_ir_optimization':a.forge_opt,'llvm_optimization':a.llvm_opt}
(out/f'results-{a.target}.json').write_text(json.dumps(report, indent=2)+'\n')
