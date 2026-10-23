#!/usr/bin/env python3
"""Time turbotuv_bench programs against each other in rotating rounds.

Each program is NAME=PATH[:ARGS]. Every round runs every program on every
configuration, starting from a different program each round, and the report
gives the median time per column. Programs whose names share a suffix after
the last '-' (for example baseline-llvm and prepared-llvm) were built with the
same compiler and must print the same hash: identical results bit for bit.
Speedups are relative to the first program with the same suffix. Programs
named with --allow-differences (for example a Fortran build whose results
differ at roundoff) are compared with the others and reported, not required
to match.
"""
import argparse
import csv
import hashlib
import json
import random
import re
import statistics
import subprocess
from pathlib import Path


def run(program, config, columns, extra):
    out = subprocess.run([program, str(config), '--columns', str(columns), *extra],
                         check=True, capture_output=True, text=True).stdout
    fields = dict(re.findall(r'(\w+)=(\S+)', out))
    return float(fields['per_column_us']), fields['hash'], fields


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def lower_bound(ratios):
    """One-sided 95% percentile bootstrap of the paired median (rounds, not columns)."""
    rng = random.Random(826109)
    medians = sorted(statistics.median(rng.choices(ratios, k=len(ratios))) for _ in range(20000))
    return medians[int(.05 * len(medians))]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('programs', nargs='+', help='NAME=PATH or NAME=PATH:ARG,ARG')
    p.add_argument('--config', type=Path, action='append', required=True)
    p.add_argument('--columns', type=int, default=300)
    p.add_argument('--rounds', type=int, default=5)
    p.add_argument('--csv', type=Path, help='write every timing to this file')
    p.add_argument('--json', type=Path, help='full samples and paired confidence bounds')
    p.add_argument('--gate', type=Path, action='append', default=[],
                   help='successful tools/reaper/compare.py evidence; validates timed binaries')
    p.add_argument('--ungated', action='store_true',
                   help='exploratory/historical comparisons only; explicitly disables acceptance')
    p.add_argument('--minimum-seconds', type=float, default=0,
                   help='reject samples shorter than this timed duration (acceptance: 1)')
    p.add_argument('--allow-differences', nargs='+', default=[], metavar='NAME',
                   help='programs whose hashes may differ from their group')
    a = p.parse_args()
    if a.rounds < 1 or a.columns < 1:
        p.error('rounds and columns must be positive')
    programs = []
    for item in a.programs:
        name, _, rest = item.partition('=')
        path, _, args = rest.partition(':')
        programs.append((name, path, [x for x in args.split(',') if x]))
    gates = [json.loads(path.read_text()) for path in a.gate]
    if a.allow_differences and not gates and not a.ungated:
        p.error('--allow-differences requires --gate, or explicit --ungated exploration')
    identities = {name: digest(path) for name, path, _ in programs}
    if gates:
        for config in a.config:
            for name, path, extra in programs:
                covered = False
                for gate in gates:
                    if not gate.get('passed') or gate.get('policy') != 'reaper-roundoff-v1':
                        continue
                    if gate['config_sha256'] != digest(config) or gate['columns'] < a.columns:
                        continue
                    seed = int(extra[extra.index('--seed') + 1]) if '--seed' in extra else 2026
                    if gate['seed'] != seed or gate['sweep'] != ('--sweep' in extra):
                        continue
                    for role in ['reference', 'candidate']:
                        record = gate[role]
                        # Execution selection must match the validated run. Repeats
                        # change sampling only; they must not change column inputs.
                        modes = ['--generic', '--reaper', '--reaper-ordered']
                        if (record['binary_sha256'] == identities[name] and
                            all((v in record['command']) == (v in extra) for v in modes)):
                            covered = True
                if not covered:
                    p.error(f'no matching successful numerical gate for {name}/{config}')
    times, hashes, rows = {}, {}, []
    samples = []
    for r in range(a.rounds):
        order = programs[r % len(programs):] + programs[:r % len(programs)]
        for config in a.config:
            for name, path, extra in order:
                us, value_hash, fields = run(path, config, a.columns, extra)
                if float(fields['elapsed_ms']) < 1000 * a.minimum_seconds:
                    raise ValueError(f'{name}: increase --repeat; timed work shorter than required')
                if gates and fields.get('profiling', '0') != '0':
                    raise ValueError('instrumented binary cannot supply acceptance timings')
                times.setdefault((name, config.stem), []).append(us)
                hashes.setdefault((name, config.stem), set()).add(value_hash)
                rows.append((r, config.stem, name, us, value_hash))
                samples.append({'round': r, 'config': str(config), 'program': name, **fields})
    if a.csv:
        with a.csv.open('w', newline='') as f:
            csv.writer(f, lineterminator='\n').writerows([('round', 'config', 'program', 'per_column_us', 'hash'), *rows])
    print(f'{a.rounds} rounds, {a.columns} columns per run; median per column (range)')
    summary = []
    for config in a.config:
        print(f'\n{config.stem}')
        reference, checked, reference_times = {}, {}, {}
        for name, _, _ in programs:
            key = (name, config.stem)
            values = times[key]
            suffix = name.rsplit('-', 1)[-1]
            reference.setdefault(suffix, statistics.median(values))
            reference_times.setdefault(suffix, values)
            assert len(hashes[key]) == 1, f'{name} printed different hashes'
            value_hash = next(iter(hashes[key]))
            note = ''
            if name in a.allow_differences:
                others = [n for n, _, _ in programs if n.rsplit('-', 1)[-1] == suffix
                          and n not in a.allow_differences]
                if others and hashes[(others[0], config.stem)] != hashes[key]:
                    note = '  (differs)'
            else:
                checked.setdefault(suffix, name)
                expected = next(iter(hashes[(checked[suffix], config.stem)]))
                assert value_hash == expected, f'{name} and {checked[suffix]} differ'
            median = statistics.median(values)
            ratios = [x / y for x, y in zip(reference_times[suffix], values)]
            bound = lower_bound(ratios)
            summary.append({'config': config.stem, 'program': name, 'median_us': median,
                            'range_us': [min(values), max(values)], 'paired_ratios': ratios,
                            'median_paired_speedup': statistics.median(ratios),
                            'speedup_lower_95': bound})
            print(f'  {name:18s} {median:9.1f} us ({min(values):.1f}-{max(values):.1f})'
                  f'  {reference[suffix] / median:5.2f}x  lower95 {bound:.3f}x hash {value_hash}{note}')
    if a.json:
        a.json.write_text(json.dumps({'schema': 1, 'gated': bool(gates) and not a.ungated,
                                     'binary_sha256': identities, 'samples': samples,
                                     'summary': summary}, indent=2) + '\n')


if __name__ == '__main__':
    main()
