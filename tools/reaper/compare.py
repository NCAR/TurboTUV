#!/usr/bin/env python3
"""Gate every benchmark output; retain field-aware roundoff and binary identity evidence.

Requires NumPy only in this development tool. Binary dumps remain disposable.
The pinned benchmark writes native doubles in column/field/row/level order,
only for the first repeat. Both binaries receive identical seed/config/columns.
Grid and label metadata are independently read through each build's CLI.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import numpy as np

FIELDS = ['direct_flux', 'down_flux', 'up_flux', 'direct_irradiance',
          'down_irradiance', 'up_irradiance', 'actinic_flux', 'spectral_irradiance',
          'photolysis', 'heating', 'dose']
POLICY = 'reaper-roundoff-v1'


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def program(spec):
    path, _, args = spec.partition(':')
    return [str(Path(path).resolve()), *filter(None, args.split(','))]


def fields(stdout):
    return dict(re.findall(r'(\w+)=(\S+)', stdout))


def metadata(command, config):
    cli = Path(command[0]).with_name('turbotuv')
    output = subprocess.check_output([str(cli), str(config), '--zenith', '30'], text=True)
    data = json.loads(output)
    return {k: v for k, v in data.items() if k != 'steps'}


def run(command, config, columns, seed, output):
    cmd = [command[0], str(config), *command[1:], '--columns', str(columns),
           '--seed', str(seed), '--output', str(output)]
    text = subprocess.check_output(cmd, text=True)
    return {'command': cmd[:-2], 'binary_sha256': sha256(command[0]),
            'fields': fields(text), 'stdout': text, 'output_sha256': sha256(output)}


def ordered(a):
    u = a.view(np.uint64)
    return np.where(u >> np.uint64(63), ~u, u | np.uint64(1 << 63))


def compare_arrays(a, b, unit_floor):
    finite = np.isfinite(a) & np.isfinite(b)
    absolute = np.abs(a - b)
    magnitude = np.maximum(np.abs(a), np.abs(b))
    relative = np.divide(absolute, magnitude, out=np.zeros_like(absolute), where=magnitude != 0)
    denominator = np.maximum(magnitude, 1.) if unit_floor else magnitude
    scaled = np.divide(absolute, denominator, out=np.zeros_like(absolute), where=denominator != 0)
    scaled /= np.finfo(np.float64).eps
    # Match scientific zero semantics: +0 and -0 compare equal, but do not
    # conceal a nonzero small rate with a unit-sized absolute tolerance.
    zero_mismatch = (a == 0) != (b == 0)
    negative_mismatch = (a >= 0) & (b < 0)
    ua, ub = ordered(a), ordered(b)
    ulp = np.maximum(ua, ub) - np.minimum(ua, ub)
    failed = ~finite | zero_mismatch | negative_mismatch | (scaled > 64)
    return {'absolute': absolute, 'relative': relative, 'epsilon': scaled, 'ulp': ulp}, failed


def compare_files(left, right, shape, columns, sweep=False):
    nz, nw = int(shape['levels']), int(shape['wavelengths'])
    rows = [nw] * 8 + [int(shape[k]) for k in FIELDS[8:]]
    if sweep:
        rows[6:8] = [0, 0]
    count = sum(rows) * nz
    expected = columns * count * 8
    if left.stat().st_size != expected or right.stat().st_size != expected:
        raise ValueError('Output byte count differs from field/shape metadata')
    a = np.memmap(left, dtype=np.float64, mode='r', shape=(columns, count))
    b = np.memmap(right, dtype=np.float64, mode='r', shape=(columns, count))
    summaries = []
    offset = 0
    for fi, (name, nr) in enumerate(zip(FIELDS, rows)):
        record = {'field': name, 'shape': [columns, nr, nz], 'compared': columns * nr * nz,
                  'failures': 0}
        if not nr:
            summaries.append(record)
            continue
        for c in range(columns):
            x = a[c, offset:offset + nr * nz].reshape(nr, nz)
            y = b[c, offset:offset + nr * nz].reshape(nr, nz)
            values, failed = compare_arrays(x, y, fi < 6)
            record['failures'] += int(failed.sum())
            if failed.any() and 'first_failure' not in record:
                row, z = np.argwhere(failed)[0]
                record['first_failure'] = {'column': c, 'row': int(row), 'level': int(z),
                                           'reference': float(x[row, z]), 'candidate': float(y[row, z])}
            for metric, v in values.items():
                index = np.unravel_index(np.nanargmax(v), v.shape) if np.isfinite(v).any() else (0, 0)
                value = int(v[index]) if metric == 'ulp' else float(v[index])
                if metric not in record or value > record[metric]['maximum']:
                    record[metric] = {'maximum': value, 'column': c, 'row': int(index[0]),
                                      'level': int(index[1]), 'reference': float(x[index]),
                                      'candidate': float(y[index])}
        summaries.append(record)
        offset += nr * nz
    return summaries


def finite_json(value):
    """Keep rejected nonfinite inputs diagnostic and standards-compliant JSON."""
    if isinstance(value, float) and not math.isfinite(value):
        return str(value)
    if isinstance(value, dict):
        return {k: finite_json(v) for k, v in value.items()}
    if isinstance(value, list):
        return [finite_json(v) for v in value]
    return value


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('reference', help='PATH[:ARG,ARG]')
    p.add_argument('candidate', help='PATH[:ARG,ARG]')
    p.add_argument('--config', type=Path, required=True)
    p.add_argument('--columns', type=int, default=300)
    p.add_argument('--seed', type=int, default=2026)
    p.add_argument('--json', type=Path, required=True)
    a = p.parse_args()
    if a.columns < 1:
        p.error('columns must be positive')
    commands = [program(a.reference), program(a.candidate)]
    report = {'schema': 1, 'policy': POLICY, 'epsilon_limit': 64,
              'config': str(a.config), 'config_sha256': sha256(a.config),
              'columns': a.columns, 'seed': a.seed, 'byteorder': sys.byteorder}
    meta = [metadata(c, a.config) for c in commands]
    if meta[0] != meta[1]:
        raise ValueError('Grid, dimension, label, or format metadata mismatch')
    report['metadata'] = meta[0]
    sweep = '--sweep' in commands[0]
    if sweep != ('--sweep' in commands[1]):
        raise ValueError('Workload mismatch')
    report['sweep'] = sweep
    with tempfile.TemporaryDirectory(prefix='turbotuv-gate-') as tmp:
        paths = [Path(tmp) / name for name in ['reference.bin', 'candidate.bin']]
        records = [run(c, a.config, a.columns, a.seed, out) for c, out in zip(commands, paths)]
        keys = ['levels', 'wavelengths', 'photolysis', 'heating', 'dose', 'columns']
        if any(records[0]['fields'][k] != records[1]['fields'][k] for k in keys):
            raise ValueError('Benchmark dimensions differ')
        report['reference'], report['candidate'] = records
        report['fields'] = compare_files(*paths, records[0]['fields'], a.columns, sweep)
    report['passed'] = all(f['failures'] == 0 for f in report['fields'])
    a.json.parent.mkdir(parents=True, exist_ok=True)
    a.json.write_text(json.dumps(finite_json(report), indent=2, allow_nan=False) + '\n')
    print(f"{'PASS' if report['passed'] else 'FAIL'}: {sum(f['compared'] for f in report['fields']):,} "
          f"values; {a.json}")
    for f in report['fields']:
        if f['failures']:
            print(f['field'], f['failures'], f.get('first_failure'))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
