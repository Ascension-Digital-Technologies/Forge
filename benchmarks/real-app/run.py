#!/usr/bin/env python3
# Copyright 2026 Mario Vinciguerra
# SPDX-License-Identifier: Apache-2.0
import argparse, json, math, pathlib, re, statistics, subprocess, time

root = pathlib.Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', default='build/release-strict')
p.add_argument('--cc', default='clang')
p.add_argument('--samples', type=int, default=15)
p.add_argument('--broad-samples', type=int, default=9)
p.add_argument('--compile-samples', type=int, default=7)
p.add_argument('--rounds', type=int, default=32)
p.add_argument('--x86-vector', choices=['sse2','sse41','avx','avx2','avx512'], default='avx512')
a = p.parse_args()

out = root/'build'/'real-app-bench'; out.mkdir(parents=True, exist_ok=True)
forge = root/a.build/'forge'
target_flags = {
    'sse2': ['-msse2'],
    'sse41': ['-msse4.1'],
    'avx': ['-mavx'],
    'avx2': ['-mavx2'],
    'avx512': ['-mavx512f','-mavx512bw','-mavx512vl'],
}[a.x86_vector]

def run(cmd, **kw):
    return subprocess.run([str(x) for x in cmd], cwd=root, check=True, **kw)

def timed(cmd):
    start=time.perf_counter(); run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return (time.perf_counter()-start)*1000.0

forge_cmd=[forge,'compile',root/'benchmarks/broad/kernels.fir','--format=elf','-O3',f'--x86-vector={a.x86_vector}','-o',out/'forge.o']
llvm_cmd=[a.cc,'-O3',*target_flags,'-c',root/'benchmarks/broad/reference.c','-o',out/'llvm.o']
run(forge_cmd)
run(llvm_cmd)
run([a.cc,'-O3',*target_flags,root/'benchmarks/real-app/harness.c',out/'forge.o',out/'llvm.o','-lm','-o',out/'integrated'])
text=subprocess.check_output([str(out/'integrated'),str(a.rounds),str(a.samples)],cwd=root,text=True)
print(text,end='')
vals={}
for line in text.splitlines():
    fields=line.split()
    if len(fields)==2: vals[fields[0]]=fields[1]
if 'checksum' not in vals or 'forge_avg_ms' not in vals or 'llvm_avg_ms' not in vals:
    raise SystemExit('integrated benchmark did not produce expected output')

# Warm once, then record process-level compile latency medians.
run(forge_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
run(llvm_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
forge_times=[timed(forge_cmd) for _ in range(a.compile_samples)]
llvm_times=[timed(llvm_cmd) for _ in range(a.compile_samples)]
fm=statistics.median(forge_times); lm=statistics.median(llvm_times)
print(f'forge_compile_median_ms {fm:.3f}')
print(f'llvm_compile_median_ms {lm:.3f}')
print(f'forge_compile_speedup {lm/fm:.3f}')

# Reuse the broad benchmark driver for the independent kernel matrix.
run(['python3',root/'benchmarks/broad/run.py','--build',a.build,'--samples',str(a.broad_samples),
     '--opt-level','O3','--x86-vector',a.x86_vector])
broad_path=root/'build'/'broad-bench'/'results-o3.json'
broad=json.loads(broad_path.read_text())
ratios=[v['ratio'] for k,v in broad.items() if not k.startswith('_')]
gm=math.exp(sum(math.log(x) for x in ratios)/len(ratios))

report={
  'target': a.x86_vector,
  'optimization': 'O3',
  'integrated_workload': {
    'samples': a.samples, 'rounds': a.rounds, 'checksum': vals['checksum'],
    'forge_avg_ms': float(vals['forge_avg_ms']), 'llvm_avg_ms': float(vals['llvm_avg_ms']),
    'forge_over_llvm': float(vals['forge_over_llvm']), 'forge_speedup': float(vals['speedup_if_forge_wins'])
  },
  'compile_time': {
    'samples': a.compile_samples, 'forge_median_ms': fm, 'llvm_median_ms': lm,
    'forge_over_llvm': fm/lm, 'forge_speedup': lm/fm
  },
  'broad_matrix': {
    'kernels': len(ratios), 'sample_pairs': a.broad_samples,
    'geometric_mean_forge_over_llvm': gm, 'geometric_mean_forge_speedup': 1.0/gm,
    'forge_text_bytes': broad['_metadata']['forge_text_bytes'],
    'llvm_text_bytes': broad['_metadata']['llvm_text_bytes']
  }
}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'broad_geomean_forge_speedup {1.0/gm:.3f}')
print(f'results {out/"results.json"}')
