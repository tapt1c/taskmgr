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
    # A six-column alpha 7 profile exercises migration in the same UI workflow.
    Path(work, 'taskmgr95.ini').write_text(
        '[Processes]\nOrder0=0\nOrder1=5\nOrder2=1\nOrder3=2\n'
        'Order4=3\nOrder5=4\nWidth1=83\n', encoding='ascii')
    result = subprocess.run([args.wine, str(helper), app], cwd=work)
raise SystemExit(result.returncode)
