#!/usr/bin/env python3
"""Capture machine, compiler, flags, immutable inputs and exact timed binary identities."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, action='append', required=True)
    p.add_argument('--reference', type=Path)
    p.add_argument('--json', type=Path, required=True)
    a = p.parse_args()
    record = {'schema': 1, 'os': platform.platform(), 'machine': platform.machine(),
              'python': platform.python_version(), 'revision': output('git', 'rev-parse', 'HEAD'),
              'frozen_main': '9e149851af5bc48fc7ce64b5fbe50c6959ff2d51',
              'floating_point': 'binary64; round to nearest; contraction off; no fast-math',
              'files': {str(p): sha(p) for p in [Path('data/manifest.json'),
                        Path('bench/turbotuv_bench.c'), *Path('examples').glob('*.json')]},
              'working_diff_sha256': hashlib.sha256(subprocess.check_output(['git', 'diff'])).hexdigest(),
              'builds': []}
    if platform.system() == 'Darwin':
        record['cpu'] = output('sysctl', '-n', 'machdep.cpu.brand_string')
        record['power'] = output('pmset', '-g', 'batt')
        record['memory_bytes'] = output('sysctl', '-n', 'hw.memsize')
    if a.reference:
        record['reference_revision'] = output('git', '-C', str(a.reference), 'rev-parse', 'HEAD')
    for build in a.build:
        cache = {}
        for line in (build / 'CMakeCache.txt').read_text().splitlines():
            if '=' in line and not line.startswith(('#', '//')):
                key, value = line.split('=', 1)
                cache[key.split(':')[0]] = value
        compiler = cache['CMAKE_C_COMPILER']
        binary = build / 'turbotuv_bench'
        inventory = json.loads(output('ctest', '--test-dir', str(build), '--show-only=json-v1'))
        record['builds'].append({'path': str(build), 'compiler': compiler,
                                'compiler_version': output(compiler, '--version'),
                                'compiler_sha256': sha(compiler),
                                'cache': {k: v for k, v in cache.items() if k.startswith(('CMAKE_C_',
                                  'CMAKE_BUILD_TYPE', 'CMAKE_OSX_', 'TURBOTUV_', 'BUILD_TESTING'))},
                                'flags': (build / 'CMakeFiles/turbotuv.dir/flags.make').read_text(),
                                'binary_sha256': sha(binary), 'binary_bytes': binary.stat().st_size,
                                'library_sha256': sha(build / 'libturbotuv.a'),
                                'tests': [t['name'] for t in inventory['tests']]})
    a.json.write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    main()
