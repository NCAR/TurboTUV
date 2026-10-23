#!/usr/bin/env python3
"""R1 isolated portable experiments. Build first, then time serial rotating rounds.

Recompile only the two translation units affected by the experiment macros,
replace those exact archive members, and link the unchanged benchmark object.
All compile/link arguments and source/binary hashes are retained in the ledger.
No generated scientific interpretation and no runtime compilation are involved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(cmd, log=None):
    result = subprocess.run(list(map(str, cmd)), check=True, capture_output=True, text=True)
    if log:
        log.write_text(result.stdout + result.stderr)
    return result.stdout


def parsed(cmd):
    return dict(re.findall(r'(\w+)=(\S+)', run(cmd)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--family', choices=['gcc', 'clang'], required=True)
    p.add_argument('--base', type=Path, required=True)
    p.add_argument('--control', type=Path, required=True)
    p.add_argument('--compiler', required=True)
    p.add_argument('--sysroot')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--columns', type=int, default=300)
    p.add_argument('--rounds', type=int, default=3)
    p.add_argument('--repeat', type=int, default=2)
    p.add_argument('--combined-only', action='store_true')
    a = p.parse_args()
    variants = [(f'tile-{n}', n, 0, 0, 0) for n in [1, 2, 4, 8, 16, 32, 0]]
    variants += [('overlap',16,1,0,0), ('oxygen',16,0,1,0), ('flux-check',16,0,0,1)]
    if a.combined_only:
        tile = 0 if a.family == 'clang' else 16
        variants = [('combined',tile,1,1,1), ('combined-separate-flux',tile,1,1,0)]
    report = {'phase': 'R1', 'family': a.family, 'source_revision': run(['git','rev-parse','HEAD']).strip(),
              'sources': {str(p): sha(p) for p in [Path('src/model_plan.c'),
                 Path('src/reaper_rates.inc'), Path('src/spectrum_plan.c'), Path('src/oxygen.c')]},
              'control_sha256': sha(a.control), 'base_library_sha256': sha(a.base/'libturbotuv.a'),
              'variants': [], 'samples': []}
    report['benchmark'] = {'columns': a.columns, 'repeat': a.repeat, 'rounds': a.rounds,
                           'seed': 2026}
    a.output.parent.mkdir(parents=True, exist_ok=True)
    commands = [('main', [str(a.control)])]
    common = [a.compiler, '-std=c11', '-O3', '-DNDEBUG', '-ffp-contract=off', '-Iinclude']
    if a.sysroot:
        common += ['-isysroot', a.sysroot]
    for name, tile, overlap, oxygen, flux in variants:
        directory = a.work / name
        directory.mkdir(parents=True, exist_ok=True)
        lib = directory / 'libturbotuv.a'
        shutil.copy2(a.base / 'libturbotuv.a', lib)
        defines = [f'-DTT_REAPER_RATE_TILE={tile}', f'-DTT_REAPER_OVERLAP={overlap}',
                   f'-DTT_REAPER_OXYGEN={oxygen}', f'-DTT_REAPER_FLUX_CHECK={flux}']
        steps = []
        start = time.monotonic()
        for unit in ['model_plan', 'spectrum_plan']:
            obj = directory / (unit + '.c.o')
            cmd = [*common, *defines, '-c', f'src/{unit}.c', '-o', str(obj)]
            steps += [cmd, ['ar', 'r', str(lib), str(obj)]]
        exe = directory / 'turbotuv_bench'
        steps += [[*common, str(a.base / 'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o'),
                   str(lib), '-lm', '-o', str(exe)]]
        for index, cmd in enumerate(steps):
            run(cmd, directory / f'build-{index}.log')
        shutil.copy2(a.base / 'turbotuv', directory / 'turbotuv')
        report['variants'].append({'name': name, 'defines': defines, 'commands': steps,
             'compile_link_seconds': time.monotonic() - start, 'binary_sha256': sha(exe),
             'binary_bytes': exe.stat().st_size,
             'object_sizes': {u: run(['size','-m',directory/(u+'.c.o')]) for u in ['model_plan','spectrum_plan']}})
        commands.append((name, [str(exe), '--reaper']))
        print(f'{a.family}: built {name}', flush=True)
    configs = ['examples/tuv_5_4.json','examples/ts1_tsmlt.json']
    hashes = {}
    for r in range(a.rounds):
        for config in configs:
            for name, cmd in commands[r % len(commands):] + commands[:r % len(commands)]:
                record = parsed([cmd[0], config, *cmd[1:], '--columns', str(a.columns),
                                 '--repeat', str(a.repeat)])
                digest = record['hash']
                if config not in hashes:
                    # Reference need not be first in subsequent rotating rounds.
                    assert name == 'main'
                    hashes[config] = digest
                if digest != hashes[config]:
                    raise ValueError(f'{name}/{config}: exact comparison failed')
                if name != 'main' and (int(record['reaper_calls']) != a.columns*a.repeat or
                                       int(record['generic_calls']) or int(record['prepared_calls'])):
                    raise ValueError(f'{name}/{config}: unexpected fallback')
                report['samples'].append({'round': r, 'variant': name, 'config': config, **record})
            print(f'{a.family}: round {r + 1}, {config}', flush=True)
        a.output.write_text(json.dumps(report, indent=2) + '\n')
    report['summary'] = []
    for config in configs:
        for name, _ in commands:
            values = [float(s['per_column_us']) for s in report['samples']
                      if s['config'] == config and s['variant'] == name]
            report['summary'].append({'config': config, 'variant': name,
                                      'median_us': statistics.median(values),
                                      'range_us': [min(values),max(values)]})
    a.output.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
