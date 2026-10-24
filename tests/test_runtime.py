"""Load the module into our trivial test EXE, never the installed rekordbox."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
fixture = root / 'build' / 'discovery-fixtures' / 'rekordbox 1' / 'rekordbox.exe'
assert fixture.is_file(), 'Run the discovery test first'
sandbox = root / 'build' / 'runtime-test'
sandbox.mkdir(parents=True, exist_ok=True)
target = sandbox / 'fixture'
target.mkdir(exist_ok=True)
shutil.copy2(fixture, target / 'rekordbox.exe')
for name in ('rekordbox-font.exe', 'font_hook.dll'):
    shutil.copy2(root / 'bin' / name, sandbox / name)
(sandbox / 'font.ini').write_text('''[Font]
Family=RBMissingFontTest
[Palette]
Enabled=0
[Window]
DarkTitleBar=0
[Diagnostics]
FontTrace=0
''', encoding='ascii')
for name in ('launcher.log', 'font-hook.log'):
    path = sandbox / name
    if path.exists():
        path.write_bytes(b'')
result = subprocess.run(
    [str(sandbox / 'rekordbox-font.exe'), '--exe', str(target / 'rekordbox.exe')],
    capture_output=True, text=True, timeout=60, check=True,
)
assert 'Font module: OK' in result.stdout, result.stdout + result.stderr
log = (sandbox / 'font-hook.log').read_text(encoding='utf-8-sig')
assert 'Font unavailable; using Segoe UI' in log, log
assert 'READY: Hooks installed' in log, log
print('PASS: module loads into the test fixture; missing font falls back to Segoe UI.')
