# Building from source

## Requirements

- Windows x64.
- Visual Studio 2022 or newer, or Visual Studio Build Tools, with **Desktop development with C++**. Include the MSVC x64/x86 tools and a Windows 10 or 11 SDK.
- Python 3 for tests. It is not needed to build or run the launcher.
- Git only if cloning the repository or creating release archives.

MinHook is included in `vendor/`. No dependency downloads, font installs or rekordbox installation are needed to compile the project. The runtime is linked statically with `/MT`.

## Build

Download `custom-launcher-for-rekordbox-<version>-source.zip` from a release and extract it, or clone the repository. Open a PowerShell or Command Prompt in the folder containing this file and run:

```powershell
.\scripts\build.cmd
```

The script finds the Visual Studio C++ tools automatically. A Developer Command Prompt isn't required. It builds:

```text
bin/rekordbox-font.exe
bin/font_hook.dll
```

`bin/font.ini` is included in the source archive and is left unchanged by the build. Keep these three files together. Close rekordbox, then run `bin/rekordbox-font.exe` to try your build. Object files and import libraries go to `build/obj/`.

## Test

With Python 3 available as `python`, run:

```powershell
.\scripts\test.cmd
```

Close rekordbox first. The tests compile trivial EXE fixtures, check version discovery, load the DLL into a fixture to test font fallback, and verify palette output in AVX2 and scalar modes. They do not launch the installed rekordbox or test its interface.

If the linker cannot write `font_hook.dll`, close the rekordbox process using that DLL and build again. If the toolchain isn't found, check that the MSVC x64/x86 tools and Windows SDK are installed in Visual Studio Installer.

## Create local release archives

This step requires a Git checkout with a clean, committed working tree. Building from an extracted source ZIP works without Git; creating source archives uses `git archive`.

After building and testing:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package.ps1 -Version 0.1.0
```

The script writes these files to `dist/`:

- `custom-launcher-for-rekordbox-0.1.0-windows-x64.zip`: EXE, DLL, default config, documentation and licenses.
- `custom-launcher-for-rekordbox-0.1.0-source.zip`: the committed source, including build scripts, tests and MinHook.
- `SHA256SUMS-0.1.0.txt`: checksums for both ZIPs.

The binary archive contains `BUILD.txt` with its source commit. Packaging uses Segoe UI without changing your local config. Existing archives with the same names are not overwritten.

## GitHub Actions releases

`.github/workflows/build.yml` builds and tests on Windows for pushes to `main`, pull requests and manual runs. These runs upload both ZIPs and their checksums as workflow artifacts.

Pushing a version tag also publishes a GitHub Release with all three files attached. For the prepared first release, push the branch and existing tag:

```powershell
git push origin main
git push origin v0.1.0
```

For a later release, commit the changes, create an annotated tag such as `v0.1.1`, and push it. Tags use `vMAJOR.MINOR.PATCH`. Release notes come from the tag message. The workflow builds the tagged commit, runs the tests, packages that same commit and checks the archive hashes before publishing.

GitHub Actions must be enabled in the repository. The release job requests `contents: write` and uses GitHub's built-in token; no personal access token is needed. Once published, use a new version tag for subsequent changes.
