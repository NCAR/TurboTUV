#!/usr/bin/env python3
"""Record instrumented stages. Never use these samples as performance acceptance."""
import argparse
import json
from pathlib import Path
import subprocess
from compare import fields, sha256
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('program')
p.add_argument('--config', action='append', required=True)
p.add_argument('--json', type=Path, required=True)
a = p.parse_args()
records = []
for config in a.config:
    cmd = [a.program, config, '--columns', '300', '--repeat', '5', '--detail']
    out = subprocess.check_output(cmd, text=True)
    records.append({'command': cmd, 'fields': fields(out), 'stdout': out})
a.json.write_text(json.dumps({'binary_sha256': sha256(a.program), 'samples': records}, indent=2) + '\n')
