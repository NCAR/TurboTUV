#!/usr/bin/env python3
"""Emit portable C11 rate kernels from the authoritative C-builder description.

No configuration parsing, spectra classification, or recomputation of physical
constants occurs here. Every double is decoded bit-exactly and emitted as a
C hexadecimal literal. Generation is deterministic; compile ahead of time.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import struct


def literal(bits):
    if not re.fullmatch('[0-9a-f]{16}', bits):
        raise ValueError('expected an IEEE binary64 bit string')
    value = struct.unpack('>d', bytes.fromhex(bits))[0]
    if not math.isfinite(value):
        raise ValueError('nonfinite baked rate constant')
    return value.hex()


def product(term, family):
    w, kind, value, energy, cross, quantum = term
    def row(x):
        return f'e{x[0]}[{x[1]} + z]'
    v = literal(value)
    if kind == 0:
        factor = v
    elif kind == 1:
        factor = f'({row(cross)} * {v})'
    elif kind == 2:
        factor = f'({v} * {row(quantum)})'
    elif kind == 3:
        factor = f'({row(cross)} * {row(quantum)})'
    else:
        raise ValueError('unknown C term kind')
    if family == 1 and kind != 0:
        factor = f'({literal(energy)} * {factor})'
    return w, factor


def generate(plan, digest, symbol, budget, accumulators, tile, interleave, use_fma, reuse=False):
    if plan['schema'] != 1 or plan['abi'] != 1 or plan['implementation'] != 1:
        raise ValueError('unsupported plan schema/ABI/implementation')
    nz, nw = plan['levels'], plan['wavelengths']
    if nz < 2 or nw < 1:
        raise ValueError('invalid dimensions')
    relaxed = accumulators != 1 or use_fma
    lines = ['/* Deterministic REAPER C11; generated from C model plan SHA-256 ' + digest + '. */',
             '#include "turbotuv_reaper.h"', '#include <math.h>']
    families = []
    for family in range(3):
        rates = [r for r in plan['rates'] if r['family'] == family]
        calls = []
        for start in range(0, len(rates), interleave):
            group = rates[start:start + interleave]
            step = max(1, budget // len(group))
            chunks = max(1, (max(len(g['terms']) for g in group) + step - 1) // step)
            for chunk in range(chunks):
                lo, hi = chunk * step, (chunk + 1) * step
                active = [(i, g) for i, g in enumerate(group) if lo < len(g['terms']) or chunk == 0]
                name = f'{symbol}_f{family}_g{start}_c{chunk}'
                calls.append(name)
                slots = sorted({row[0] for _, g in active for t in g['terms'][lo:hi]
                                for row in t[4:6] if row is not None})
                lines += [f'static void {name}(const tt_reaper_view *v) {{',
                          f'    const double *restrict f = v->{"irradiance" if family == 2 else "actinic"};',
                          '    double *restrict out = v->rates;']
                for slot in slots:
                    lines.append(f'    const double *e{slot} = v->spectra[{slot}];')
                if tile:
                    lines += [f'    for (size_t z0 = 0; z0 < {nz}; z0 += {tile}) {{',
                              f'      size_t end = z0 + {tile} < {nz} ? z0 + {tile} : {nz};',
                              '      for (size_t z = z0; z < end; ++z) {']
                else:
                    lines.append(f'    for (size_t z = 0; z < {nz}; ++z) {{')
                for i, g in active:
                    for acc in range(accumulators):
                        initial = f'out[{g["output"]} + z]' if not relaxed and chunk else '0.0'
                        lines.append(f'        double s{i}_{acc} = {initial};')
                products = Counter(product(t, family)[1] for _, g in active for t in g['terms'][lo:hi] if t[1])
                reusable = {factor: f'p{i}' for i, (factor, count) in enumerate(products.items()) if count > 1} if reuse else {}
                for factor, variable in reusable.items():
                    lines.append(f'        const double {variable} = {factor};')
                for j in range(step):
                    for i, g in active:
                        if lo + j >= len(g['terms']):
                            continue
                        w, factor = product(g['terms'][lo + j], family)
                        factor = reusable.get(factor, factor)
                        flux = f'f[{w * nz} + z]'
                        acc = f's{i}_{j % accumulators}'
                        rhs = f'fma({flux}, {factor}, {acc})' if use_fma else f'{acc} + {flux} * {factor}'
                        lines.append(f'        {acc} = {rhs};')
                for i, g in active:
                    result = f's{i}_0'
                    for acc in range(1, accumulators):
                        result = f'({result} + s{i}_{acc})'
                    if relaxed and chunk:
                        result = f'(out[{g["output"]} + z] + {result})'
                    if hi >= len(g['terms']) and family != 2:
                        result = f'({result}) * {literal(g["scale"])}'
                    lines.append(f'        out[{g["output"]} + z] = {result};')
                lines += ['    }'] + (['    }'] if tile else []) + ['}']
        if not rates:
            families.append('NULL')
            continue
        function = f'{symbol}_family{family}'
        families.append(function)
        lines += [f'static int {function}(const tt_reaper_view *v) {{',
                  f'    if (v->levels != {nz} || v->wavelengths != {nw} || v->slots != {len(plan["slots"]) + 1}) return 0;']
        lines += [f'    {name}(v);' for name in calls]
        lines += ['    return 1;', '}']
    bytes_ = ','.join('0x' + digest[i:i + 2] for i in range(0, 64, 2))
    nr, nh, nd = plan['outputs']
    lines += [f'const tt_reaper_kernels {symbol} = {{',
              f'    TT_REAPER_ABI, TT_REAPER_SCHEMA, TT_REAPER_IMPLEMENTATION, {"TT_REAPER_ROUNDOFF" if relaxed else "TT_REAPER_ORDERED"},',
              f'    {nz}, {nw}, {len(plan["slots"]) + 1}, {nr}, {nh}, {nd}, {budget}, {accumulators}, {tile},',
              f'    {{{bytes_}}},', f'    {", ".join(families)}', '};', '']
    return '\n'.join(lines)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('plan', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--symbol', required=True)
    p.add_argument('--term-budget', type=int, choices=[32,64,128,256], default=64)
    p.add_argument('--accumulators', type=int, choices=[1,2,4,8], default=1)
    p.add_argument('--level-tile', type=int, choices=[0,1,2,4,8,16,32], default=0)
    p.add_argument('--interleave', type=int, choices=[1,2,4,8], default=1)
    p.add_argument('--fma', action='store_true')
    p.add_argument('--reuse-products', action='store_true')
    a = p.parse_args()
    if not re.fullmatch('[A-Za-z_][A-Za-z0-9_]*', a.symbol):
        p.error('invalid C identifier')
    raw = a.plan.read_bytes()
    code = generate(json.loads(raw), hashlib.sha256(raw).hexdigest(), a.symbol,
                    a.term_budget, a.accumulators, a.level_tile, a.interleave, a.fma, a.reuse_products)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(code)
    print(json.dumps({'plan_sha256': hashlib.sha256(raw).hexdigest(),
                      'source_sha256': hashlib.sha256(code.encode()).hexdigest(),
                      'source_bytes': len(code), 'symbol': a.symbol,
                      'term_budget': a.term_budget, 'accumulators': a.accumulators,
                      'level_tile': a.level_tile, 'interleave': a.interleave, 'fma': a.fma,
                      'reuse_products': a.reuse_products}))


if __name__ == '__main__':
    main()
