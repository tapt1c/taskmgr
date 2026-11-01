#!/usr/bin/env python3
"""Run the single UI workflow without overwriting a developer's preferences."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--wine', default='wine')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
helper = root / 'build/wine64/gui_smoke.exe'
app = 'Z:' + str(root / 'build/wine64/taskmgr95-ui.exe').replace('/', '\\')
with tempfile.TemporaryDirectory(prefix='tm95-ui-') as work:
    result = subprocess.run([args.wine, str(helper), app], cwd=work)
raise SystemExit(result.returncode)
