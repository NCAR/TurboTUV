#!/usr/bin/env python3
"""Build optional static C11 rate kernels. No compiler is invoked by the library.

Input plans must be exported by the target library's C model builder. The
returned sources can also be supplied to CMake's TURBOTUV_REAPER_SOURCES.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time
from generate import generate


def build(plans, directory, cc, flags, budget=64, accumulators=1, tile=0,
          interleave=1, fma=False, reuse=False):
    directory.mkdir(parents=True, exist_ok=True)
    record = {'plans': [], 'commands': [], 'sources': [], 'objects': []}
    symbols = []
    started = time.monotonic()
    for i, path in enumerate(plans):
        raw = Path(path).read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        symbol = f'tt_generated_{i}'
        symbols.append(symbol)
        source = directory / f'kernel-{i}.c'
        code = generate(json.loads(raw), digest, symbol, budget, accumulators, tile, interleave, fma, reuse)
        source.write_text(code)
        record['sources'].append(str(source))
        record['plans'].append({'path': str(path), 'sha256': digest,
                                'source_sha256': hashlib.sha256(code.encode()).hexdigest(),
                                'source_bytes': len(code)})
    registry = directory / 'registry.c'
    registry.write_text('#include "turbotuv_reaper.h"\n' + '\n'.join(
        f'extern const tt_reaper_kernels {s};' for s in symbols) +
        '\nconst tt_reaper_kernels *const *tt_reaper_builtin_tables(size_t *count) {\n' +
        '    static const tt_reaper_kernels *const tables[] = {' +
        ','.join('&' + s for s in symbols) + '};\n' +
        '    *count = sizeof tables / sizeof *tables;\n    return tables;\n}\n')
    record['sources'].append(str(registry))
    for name in record['sources']:
        source = Path(name)
        obj = source.with_suffix('.o')
        cmd = [cc, *flags, '-c', str(source), '-o', str(obj)]
        record['commands'].append(cmd)
        result = subprocess.run(cmd, capture_output=True, text=True)
        source.with_suffix('.log').write_text(result.stdout + result.stderr)
        result.check_returncode()
        record['objects'].append(str(obj))
    record['compile_seconds'] = time.monotonic() - started
    record['object_bytes'] = sum(Path(o).stat().st_size for o in record['objects'])
    return record


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('plans', type=Path, nargs='+')
    p.add_argument('--directory', type=Path, required=True)
    p.add_argument('--cc', required=True)
    p.add_argument('--sysroot')
    p.add_argument('--term-budget', type=int, default=64, choices=[32,64,128,256])
    p.add_argument('--accumulators', type=int, default=1, choices=[1,2,4,8])
    p.add_argument('--level-tile', type=int, default=0, choices=[0,1,2,4,8,16,32])
    p.add_argument('--interleave', type=int, default=1, choices=[1,2,4,8])
    p.add_argument('--fma', action='store_true')
    p.add_argument('--reuse-products', action='store_true')
    a = p.parse_args()
    flags = ['-std=c11','-O3','-fPIC','-DNDEBUG','-ffp-contract=off','-Iinclude']
    if a.sysroot:
        flags += ['-isysroot',a.sysroot]
    r = build(a.plans, a.directory, a.cc, flags, a.term_budget, a.accumulators,
              a.level_tile, a.interleave, a.fma, a.reuse_products)
    (a.directory / 'artifact.json').write_text(json.dumps(r, indent=2) + '\n')
    print(json.dumps(r))


if __name__ == '__main__':
    main()
