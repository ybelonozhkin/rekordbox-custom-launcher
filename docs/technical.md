# Settings and implementation

## Configuration

`font.ini` is read from the DLL's directory at startup. In a source checkout this is `bin/`; release ZIPs place it beside the EXE and DLL. Restart to apply changes.

| Section | Key | Effect |
| --- | --- | --- |
| Font | Family | Installed font family; Segoe UI if missing or unavailable |
| Palette | Enabled | `0` disables color processing |
| Palette | BlackLevel | Darkest neutral shade, clamped to 0–40; default 24 |
| Palette | WhiteLevel | Brightest neutral shade, clamped to 160–255; default 192 |
| Palette | HighlightColor | Replacement highlight color as `#RRGGBB`; empty disables recoloring |
| Palette | HighlightStrength | Recoloring strength, 0–100; default 100 |
| Palette | HighlightSourceColor | Original RGB color to match; empty disables replacement |
| Palette | TintColor | Overlay color as `#RRGGBB` or `RRGGBB`; default `#000000` |
| Palette | TintStrength | Overlay strength, 0–100; default 0 (off) |
| Window | DarkTitleBar | `1` requests a dark native title bar; default 1 |
| Diagnostics | FontTrace | `1` logs font requests; default 0 |

The local `Input` preset was downloaded through the font author's custom-family builder: Input Sans Compressed Light for Regular/Italic, and Input Sans Compressed Regular for Bold/Bold Italic. Its internal family name is `Input`. It isn't bundled. Release packages use Segoe UI instead, without changing the local config.

For a subtle warm tint, try `TintColor=#C88B55` and `TintStrength=5`. The RGB overlay is blended after the existing palette transform. A strength of 100 replaces the processed image with a flat color; use small values for a readable interface. An invalid color disables the overlay. The same tint is applied to adjusted dark OpenGL backgrounds. It can affect artwork and waveform images too.

Color replacement is defined by `HighlightSourceColor`, `HighlightColor` and `HighlightStrength`. The source and target can be any RGB colors. There are no color-specific processing rules. The shipped configuration maps `#1473E6` to `#1C5AA2` at strength 100; these are editable config values, not special cases in the algorithm.

With `HighlightStrength=100`, the exact source color maps to the exact target before the overall tint. Lower strengths blend with the neutral-adjusted image. Darker shades and antialiased edges retain their neutral contribution. Empty or invalid source/target colors disable replacement; strength 0 also leaves the neutral-adjusted image unchanged.

Colored sources match nearby hues with a fixed tolerance, including lighter and darker shades; neutral sources match only that exact RGB shade. This is a pixel transform, not focus tracking: any image pixels in the same color range can be affected. Replacement runs after the neutral transform and before the overall tint. The gray lookup table uses the same transform as the scalar path.

## Launcher

Installation discovery checks Program Files, uninstall registry entries and App Paths. Candidates must be named `rekordbox.exe`, identify as rekordbox in their version resources, and be x64. Versions are compared numerically using FileVersion. Discovery does not execute candidate binaries.

The launcher starts the selected EXE suspended, loads `font_hook.dll` through `LoadLibraryW` in the child, calls its `Initialize` export, then resumes the app. If initialization fails, it reports the failure and resumes rekordbox normally. It clears an inherited `ELECTRON_RUN_AS_NODE` variable in its own environment so rekordbox's Electron companion can start normally. It doesn't change system settings.

`--exe PATH` selects an installation. `--list` lists detected installations; `--list --root PATH` limits discovery to a test directory. `--attach PID` is a developer command for loading the module into an already-running supported target.

## Rendering

MinHook intercepts DirectWrite `FindFamilyName` and `CreateTextFormat`, plus GDI `CreateFontIndirectW`. Arial and Arial Unicode MS requests are redirected to the configured family. There is no per-widget selection: deck numbers, controls and library text can all change. Fixed-width fields may truncate wider fonts.

The palette adjusts neutral shades, replaces a configurable source color and applies an optional RGB tint to eligible 32-bit GDI bitmaps passed to `StretchDIBits`. Dark neutral OpenGL clear colors are adjusted too. AVX2 is used when the CPU and OS support it; other machines use a scalar path. This processes some already-rendered images, so covers and waveform thumbnails can change. It isn't a semantic theme engine. Color processing adds CPU work; the cost depends on the redrawn area.

A startup worker requests a dark DWM title bar after the main window appears. It exits after the first attempt on that window, or a 60-second timeout. There is no permanent window polling, cursor manipulation or background helper app.

## Logs and checks

`launcher.log`, `font-hook.log`, `palette.log` and `titlebar.log` are written beside the binaries. Font tracing is disabled by default; logs can contain paths to the installation. Review them before sharing.

`scripts/test.cmd` checks installation discovery with fake version resources and compares all 16,777,216 RGB palette outputs against the scalar implementation. It also tests the scalar fallback and loads the DLL into a trivial test EXE to check the missing-font fallback. It does not launch the installed rekordbox or verify its UI. Close rekordbox before running the runtime test.

For performance comparisons, close screen automation and accessibility inspection tools. On the development machine these caused scrolling and resizing lag even with an unmodified rekordbox. Compare palette on/off under the same conditions.

## Packaging

`scripts/package.ps1` uses an explicit file list for binary releases. It includes the EXE, DLL, a Segoe UI config, README, CHANGELOG, MIT license, third-party notices and the MinHook license. Source ZIPs come from the current Git commit and require a clean working tree. Personal files, fonts, logs and application artwork stay out.

The GitHub workflow builds and tests on Windows, then uploads both archives and their SHA-256 checksums. Tags named `v*` publish those files as a GitHub Release.
