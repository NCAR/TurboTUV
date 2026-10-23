#!/usr/bin/env python3
"""Verify spectral exponential constants and a uniform binary64 error bound."""
import argparse
from decimal import Decimal, localcontext, ROUND_CEILING
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--header', type=Path, default=Path('src/reaper_math.h'))
p.add_argument('--table-oracle', type=Path, help='optional compiled check_math_table.c')
p.add_argument('--json', type=Path, required=True)
a = p.parse_args()
source = a.header.read_text()
hex_pattern = r'0x[0-9a-f.]+p[+-]\d+'
match = re.search(r'static const double powers\[(\d+)\] = \{(.*?)\};', source, re.S)
n = int(match[1])
assert n in (64, 256, 512, 1024, 2048)
macro = re.search(r"#define TT_REAPER_EXP_TABLE_SIZE (\d+)", source)
assert macro is None or int(macro[1]) == n
values = re.findall(hex_pattern, match[2])
assert len(values) == n
split = re.search(r'r = fma\(-\(double\)k, (' + hex_pattern +
                  r'), x - \(double\)k \* (' + hex_pattern + r')\);', source)
assert split
low_hex, high_hex = split.groups()
reciprocals = re.search(r'double q = x \* \(base10 \? (' + hex_pattern +
                        r') : (' + hex_pattern + r')\);', source)
assert reciprocals
poly = source[source.index('        double p = '):source.index('        uint64_t index')]
coefficients = re.findall(hex_pattern, poly)
degree = len(coefficients) - 1
assert degree in (3, 4, 5) and poly.count('fma(') == degree
assert (f'index & {n - 1}' in source and f'index >> {n.bit_length() - 1}' in source
        and f'k + {n * 1024}' in source)
int_limit = int(re.search(r'INT_MAX >= (\d+)', source)[1])


def decimal_hex(value):
    return Decimal.from_float(float.fromhex(value))


def significant_bits(value):
    numerator = abs(float.fromhex(value).as_integer_ratio()[0])
    while numerator and numerator % 2 == 0:
        numerator //= 2
    return numerator.bit_length()


with localcontext() as context:
    context.prec = 120
    epsilon = Decimal(2) ** -52
    u, tiny = epsilon / 2, Decimal(2) ** -1074
    ln2, ln10 = Decimal(2).ln(), Decimal(10).ln()
    h = ln2 / n
    high, low = decimal_hex(high_hex), decimal_hex(low_hex)
    k_bound = int((Decimal(600) / h).to_integral_value(rounding=ROUND_CEILING))
    product_bits = k_bound.bit_length() + significant_bits(high_hex)
    assert int_limit >= k_bound and product_bits <= 53
    assert abs((decimal_hex(reciprocals[2]) - 1 / h) / (1 / h)) <= u
    assert abs((decimal_hex(reciprocals[1]) - ln10 / h) / (ln10 / h)) <= u
    ln10_high_hex, ln10_low_hex = '0x1.26bb1bbb00000p+1', '0x1.554560b752b6bp-33'
    assert ln10_high_hex in source and ln10_low_hex in source
    ln10_high, ln10_low = decimal_hex(ln10_high_hex), decimal_hex(ln10_low_hex)
    assert 0 < ln10_high < ln10

    # k=1 limits the Sterbenz ratio. Its selection error is below 12*u:
    # the exact argument divided by h is below 1.5 at this boundary.
    assert (Decimal('.5') - 12 * u) * h * (ln10_high / ln10) * (1 - u) >= high / 2
    # For k>=2 the lower ratio is at least 0.75, with ample margin.
    assert (Decimal('.75') - 4 * u * (k_bound + 1)) * h * (ln10_high / ln10) * (1 - u) >= high / 2
    assert (Decimal('1.5') + 4 * u * (k_bound + 1)) * h * (1 + u) <= 2 * high
    residual = h * (Decimal('.5') + 4 * u * (k_bound + 1))
    interval = h / 2 + Decimal('1e-12')
    assert residual < interval
    split_error = abs(high + low - h)
    exp_c = residual + k_bound * abs(h - high)
    exp_error = k_bound * split_error + u * (exp_c + k_bound * abs(low)) + 8 * tiny

    # A = round(x*ln10_high); its FMA residual is exact apart from
    # minimum-subnormal terms. B adds x*ln10_low, C = A-k*high is exact,
    # D rounds C+B, and the final FMA subtracts k*low.
    product_error = u * 260 * abs(ln10_high)
    b_exact = 260 * abs(ln10_low) + product_error
    b_error = u * b_exact + 2 * tiny
    c_bound = residual + 260 * abs(ln10 - ln10_high) + k_bound * abs(h - high) + product_error
    d_error = u * (c_bound + b_exact + b_error) + 2 * tiny
    d_bound = (c_bound + b_exact + b_error) * (1 + u) + 2 * tiny
    final_error = u * (d_bound + k_bound * abs(low)) + 2 * tiny
    pow_error = (260 * abs(ln10_high + ln10_low - ln10) + k_bound * split_error
                 + b_error + d_error + final_error + 8 * tiny)
    reduction = max(exp_error, pow_error)
    assert residual + reduction < interval

    for i, value in enumerate(values):
        assert float((ln2 * i / n).exp()).hex() == float.fromhex(value).hex()
    for index, value in enumerate(coefficients):
        exact = Decimal(1) / Decimal(math.factorial(degree - index))
        assert abs(decimal_hex(value) - exact) <= u * exact
    gamma = degree * u / (1 - degree * u)
    arithmetic = gamma * (2 * interval).exp()
    coefficient_error = u * (2 * interval).exp()
    remainder = ((2 * interval).exp() * interval ** (degree + 1)
                 / Decimal(math.factorial(degree + 1)))
    polynomial = arithmetic + coefficient_error + remainder
    combined = reduction.exp() * (1 + polynomial) * (1 + u) ** 2 - 1
    assert combined / epsilon < 5
    result = {
        'header_sha256': hashlib.sha256(source.encode()).hexdigest(),
        'decimal_precision': 120, 'table': n, 'degree': degree, 'k_bound': k_bound,
        'high_significant_bits': significant_bits(high_hex),
        'exact_product_bits_bound': product_bits, 'residual_bound': str(interval),
        'exp_reduction_absolute': str(exp_error), 'pow10_reduction_absolute': str(pow_error),
        'conservative_eps': {
            'reduction': str((reduction.exp() - 1) / epsilon),
            'horner': str(arithmetic / epsilon),
            'coefficients': str(coefficient_error / epsilon),
            'remainder': str(remainder / epsilon),
            'table_and_product_first_order': '1',
        },
        'sum_eps': str(combined / epsilon), 'construction_verified': True,
        'table_entries_verified': n,
        'note': 'Uniform binary64 nearest-rounding bound. Derivation: docs/REAPER_MATH.md.',
    }
if a.table_oracle:
    output = subprocess.check_output([str(a.table_oracle), str(n)],
                                     input='\n'.join(values) + '\n', text=True)
    result['mpfr_table'] = json.loads(output)
    assert result['mpfr_table']['checked'] == n and result['mpfr_table']['exact']
    result['table_oracle_sha256'] = hashlib.sha256(a.table_oracle.read_bytes()).hexdigest()
a.json.write_text(json.dumps(result, indent=2) + '\n')
print(f'table={n} degree={degree} bound_epsilon={result["sum_eps"]}')
