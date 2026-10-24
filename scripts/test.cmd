@echo off
setlocal
call "%~dp0toolchain.cmd"
if errorlevel 1 exit /b 1
set "RB_ROOT=%~dp0.."
if not exist "%RB_ROOT%\bin\rekordbox-font.exe" (
    echo Run scripts\build.cmd first. 1>&2
    exit /b 1
)
python "%RB_ROOT%\tests\test_discovery.py"
if errorlevel 1 exit /b 1
python "%RB_ROOT%\tests\test_runtime.py"
if errorlevel 1 exit /b 1
pushd "%RB_ROOT%\build\obj"
cl /nologo /O2 /MT /EHsc /std:c++17 "%RB_ROOT%\tests\benchmark_palette.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /Fe:"%RB_ROOT%\build\benchmark_palette.exe" /link gdi32.lib user32.lib
if errorlevel 1 goto failed
"%RB_ROOT%\build\benchmark_palette.exe"
if errorlevel 1 goto failed
"%RB_ROOT%\build\benchmark_palette.exe" --scalar
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
