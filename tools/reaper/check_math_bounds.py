#!/usr/bin/env python3
"""Verify the bounded exponential constants and the numerical bound table."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
import math
from pathlib import Path
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--header', type=Path, default=Path('src/reaper_math.h'))
p.add_argument('--json', type=Path, required=True)
a = p.parse_args()
source = a.header.read_text()
table = source.split('static const double powers[64] = {', 1)[1].split('};', 1)[0]
values = re.findall(r'0x[0-9a-f.]+p[+-]\d+', table)
assert len(values) == 64
with localcontext() as context:
    context.prec = 100
    for i, value in enumerate(values):
        expected = float((Decimal(2).ln() * Decimal(i) / 64).exp())
        assert expected.hex() == float.fromhex(value).hex()
    epsilon = Decimal(2) ** -52
    u = epsilon / 2
    r = Decimal('.00542')
    bounds = {
        'reduction': Decimal('.126'),
        'horner': 5*u/(1-5*u)*(2*r).exp()/epsilon,
        'coefficients': u*(2*r).exp()/epsilon,
        'remainder': (2*r).exp()*r**6/Decimal(math.factorial(6))/epsilon,
        'table': u/epsilon,
        'table_product': u/epsilon,
    }
    assert sum(bounds.values()) < 5
    result = {
        'header_sha256': hashlib.sha256(source.encode()).hexdigest(),
        'decimal_precision': 100,
        'table_entries_verified': len(values),
        'conservative_eps': {k: str(v) for k, v in bounds.items()},
        'sum_eps': str(sum(bounds.values())),
        'note': 'Reduction and arithmetic derivation: docs/REAPER_MATH.md.',
    }
a.json.write_text(json.dumps(result, indent=2) + '\n')
