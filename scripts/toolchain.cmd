@echo off
rem Called by build/test scripts; keep the configured environment in the caller.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio C++ Build Tools and the Windows SDK are required. 1>&2
    exit /b 1
)
set "RB_VS_ROOT="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "RB_VS_ROOT=%%I"
if not defined RB_VS_ROOT (
    echo No Visual Studio installation with x64 C++ tools was found. 1>&2
    exit /b 1
)
call "%RB_VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
