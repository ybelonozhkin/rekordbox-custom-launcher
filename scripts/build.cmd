@echo off
setlocal
call "%~dp0toolchain.cmd"
if errorlevel 1 exit /b 1
set "RB_ROOT=%~dp0.."
if not exist "%RB_ROOT%\build\obj" mkdir "%RB_ROOT%\build\obj"
if not exist "%RB_ROOT%\bin" mkdir "%RB_ROOT%\bin"
pushd "%RB_ROOT%\build\obj"
cl /nologo /c /O2 /MT /I "%RB_ROOT%\vendor\minhook\include" "%RB_ROOT%\vendor\minhook\src\buffer.c" "%RB_ROOT%\vendor\minhook\src\hook.c" "%RB_ROOT%\vendor\minhook\src\trampoline.c" "%RB_ROOT%\vendor\minhook\src\hde\hde64.c"
if errorlevel 1 goto failed
cl /nologo /LD /O2 /MT /EHsc /std:c++17 "%RB_ROOT%\src\font_hook.cpp" "%RB_ROOT%\src\palette.cpp" "%RB_ROOT%\src\titlebar.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:"%RB_ROOT%\bin\font_hook.dll" /IMPLIB:font_hook.lib dwrite.lib gdi32.lib version.lib dwmapi.lib user32.lib
if errorlevel 1 goto failed
cl /nologo /O2 /MT /EHsc /std:c++17 "%RB_ROOT%\src\launcher.cpp" /Fe:"%RB_ROOT%\bin\rekordbox-font.exe" /link version.lib advapi32.lib user32.lib
if errorlevel 1 goto failed
popd
echo Built bin\rekordbox-font.exe and bin\font_hook.dll. Existing font.ini preserved.
exit /b 0
:failed
popd
exit /b 1
