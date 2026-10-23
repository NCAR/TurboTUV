#!/usr/bin/env python3
"""Measure fresh-model static startup and repeated selection, outside column timing.

Break-even estimates use separately gated steady-state samples. These short
startup runs are not evidence for a steady-state speedup or an OS-cold cache.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess


def sample(command, env=None):
    output = subprocess.check_output(list(map(str, command)), text=True, env=env)
    return {'command': list(map(str, command)), 'stdout': output,
            'fields': dict(re.findall(r'(\w+)=(\S+)', output))}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--main', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--artifact', type=Path, required=True)
    p.add_argument('--steady', type=Path, required=True)
    p.add_argument('--family', required=True)
    p.add_argument('--config', type=Path, action='append', required=True)
    p.add_argument('--json', type=Path, required=True)
    a = p.parse_args()
    artifact = json.loads(a.artifact.read_text())
    steady = json.loads(a.steady.read_text())
    assert steady['gated']
    record = {'samples': [], 'summary': [], 'steady_source': str(a.steady),
              'artifact': str(a.artifact), 'binary_sha256': {
                  str(x): hashlib.sha256(x.read_bytes()).hexdigest()
                  for x in [a.main, a.candidate]}}
    for config in a.config:
        samples = {'main': [], 'candidate': []}
        for i in range(5):
            programs = [('main', a.main, []), ('candidate', a.candidate, ['attached'])]
            for name, binary, extra in programs[::1 if i % 2 == 0 else -1]:
                env = dict(os.environ, PATH='/nonexistent')
                item = sample([binary.resolve(), config.resolve(), *extra], env)
                item.update(config=config.stem, program=name, round=i, path='/nonexistent')
                samples[name].append(item['fields'])
                record['samples'].append(item)
        times = {name: statistics.median(float(x['setup_ms']) + float(x.get('preparation_ms', 0))
                                         for x in samples[name]) for name in ['main', 'candidate']}
        medians = {x['program']: x['median_us'] for x in steady['summary'] if x['config'] == config.stem}
        saved = medians['main-' + a.family] - medians['reaper-' + a.family]
        extra = max(0, times['candidate'] - times['main'])
        record['summary'].append({'config': config.stem, 'initialization_ms': times,
            'extra_initialization_ms': extra, 'steady_saved_us': saved,
            'static_break_even_columns': math.ceil(extra * 1000 / saved) if saved > 0 else None,
            'with_emission_compile_columns': math.ceil((extra * .001 + artifact['compile_seconds'])
                                                       / (saved * 1.e-6)) if saved > 0 else None,
            'median_repeat_selection_ns': statistics.median(float(x['reuse_ns']) for x in samples['candidate']),
            'note': 'Estimate uses R3 gated latency and R4 bit-identical outputs; emission/compile excludes final linking.'})
    a.json.write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    main()
