#!/usr/bin/env python3
"""Optional macOS sampling of an untimed control; never sample acceptance runs."""
import argparse
import json
from pathlib import Path
import subprocess
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('program')
p.add_argument('--config', required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
cmd = [a.program, a.config, '--columns', '300', '--repeat', '100']
proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL)
try:
    sample = subprocess.run(['/usr/bin/sample', str(proc.pid), '2', '1', '-file', str(a.output)],
                            capture_output=True, text=True)
    print(json.dumps({'command': cmd, 'sample_status': sample.returncode,
                      'sample_diagnostic': sample.stderr}, indent=2))
    sample.check_returncode()
finally:
    proc.terminate()
    proc.wait()
