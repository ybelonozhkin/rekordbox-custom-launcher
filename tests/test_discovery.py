from pathlib import Path
import subprocess

tests = Path(__file__).resolve().parent
root = tests.parent
fixtures = root / 'build' / 'discovery-fixtures'
fixtures.mkdir(parents=True, exist_ok=True)
cases = [('rekordbox 99', '7,2,9,0', 'rekordbox'),
         ('rekordbox 1', '7,2,19,0', 'rekordbox'),
         ('unrelated', '99,0,0,0', 'Another application')]
commands = ['@echo off', f'call "{root / "scripts" / "toolchain.cmd"}"', 'if errorlevel 1 exit /b 1']
for name, version, product in cases:
    folder = fixtures / name
    folder.mkdir(exist_ok=True)
    resource = folder / 'version.rc'
    resource.write_text(f'''#include <windows.h>
1 VERSIONINFO
 FILEVERSION {version}
 PRODUCTVERSION {version}
 FILEFLAGSMASK 0x3fL
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_APP
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904b0"
  BEGIN
   VALUE "ProductName", "{product}\\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x409, 1200
 END
END
''', encoding='ascii')
    commands += [f'rc /nologo /fo "{folder / "version.res"}" "{resource}"',
                 'if errorlevel 1 exit /b 1',
                 f'cl /nologo /MT "{tests / "discovery_fixture.cpp"}" /Fo"{folder / "fixture.obj"}" /Fe"{folder / "rekordbox.exe"}" /link "{folder / "version.res"}"',
                 'if errorlevel 1 exit /b 1']
build = root / 'build' / 'build-fixtures.cmd'
build.write_text('\n'.join(commands)+'\n', encoding='ascii')
subprocess.run(['cmd', '/c', str(build)], check=True, cwd=root)
result = subprocess.run([str(root / 'bin' / 'rekordbox-font.exe'), '--list', '--root', str(fixtures)], check=True, capture_output=True, text=True)
lines = result.stdout.strip().splitlines()
print(result.stdout, end='')
assert len(lines) == 2, 'Unrelated product should be rejected'
assert lines[0].startswith('SELECTED 7.2.19.0 '), 'Numeric ordering failed'
assert lines[1].startswith('FOUND 7.2.9.0 '), 'Older version must be second'
print('PASS: file versions ordered numerically; folder names ignored; unrelated product rejected. Discovery only, fixtures not executed.')
