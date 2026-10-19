#!/usr/bin/env python3
"""Time turbotuv_bench programs against each other in rotating rounds.

Each program is NAME=PATH[:ARGS]. Every round runs every program on every
configuration, starting from a different program each round, and the report
gives the median time per column. Programs whose names share a suffix after
the last '-' (for example baseline-llvm and prepared-llvm) were built with the
same compiler and must print the same hash: identical results bit for bit.
Speedups are relative to the first program with the same suffix.
"""
import argparse
import csv
import re
import statistics
import subprocess
from pathlib import Path


def run(program, config, columns, extra):
    out = subprocess.run([program, str(config), '--columns', str(columns), *extra],
                         check=True, capture_output=True, text=True).stdout
    fields = dict(re.findall(r'(\w+)=(\S+)', out))
    return float(fields['per_column_us']), fields['hash']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('programs', nargs='+', help='NAME=PATH or NAME=PATH:ARG,ARG')
    p.add_argument('--config', type=Path, action='append', required=True)
    p.add_argument('--columns', type=int, default=300)
    p.add_argument('--rounds', type=int, default=5)
    p.add_argument('--csv', type=Path, help='write every timing to this file')
    a = p.parse_args()
    programs = []
    for item in a.programs:
        name, _, rest = item.partition('=')
        path, _, args = rest.partition(':')
        programs.append((name, path, [x for x in args.split(',') if x]))
    times, hashes, rows = {}, {}, []
    for r in range(a.rounds):
        order = programs[r % len(programs):] + programs[:r % len(programs)]
        for config in a.config:
            for name, path, extra in order:
                us, digest = run(path, config, a.columns, extra)
                times.setdefault((name, config.stem), []).append(us)
                hashes.setdefault((name, config.stem), set()).add(digest)
                rows.append((r, config.stem, name, us, digest))
    if a.csv:
        with a.csv.open('w', newline='') as f:
            csv.writer(f).writerows([('round', 'config', 'program', 'per_column_us', 'hash'), *rows])
    print(f'{a.rounds} rounds, {a.columns} columns per run; median per column (range)')
    for config in a.config:
        print(f'\n{config.stem}')
        reference = {}
        for name, _, _ in programs:
            key = (name, config.stem)
            values = times[key]
            suffix = name.rsplit('-', 1)[-1]
            reference.setdefault(suffix, (name, statistics.median(values)))
            base_name, base = reference[suffix]
            digests = hashes[key] | hashes[(base_name, config.stem)]
            assert len(hashes[key]) == 1, f'{name} printed different hashes'
            assert len(digests) == 1, f'{name} and {base_name} differ: {sorted(digests)}'
            median = statistics.median(values)
            print(f'  {name:18s} {median:9.1f} us ({min(values):.1f}-{max(values):.1f})'
                  f'  {base / median:5.2f}x  hash {next(iter(hashes[key]))}')


if __name__ == '__main__':
    main()
