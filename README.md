# Custom launcher for rekordbox

A different font, softer colors and a dark title bar for rekordbox on Windows.

The launcher loads a small DLL into rekordbox. It doesn't patch installed files or touch your music or database. Start rekordbox normally to use its original look.

The native window title bar also matches a dark Windows theme. This is enabled by default with `DarkTitleBar=1` in the `[Window]` section of `font.ini`; set it to `0` to leave the title bar unchanged.

## Use

1. Download and extract the Windows x64 ZIP from Releases.
2. Close rekordbox, then run `rekordbox-font.exe` from the extracted folder.
3. To change the font, edit `font.ini` beside the EXE and restart through the launcher.

Release ZIPs use Segoe UI by default. Choose the family name of any installed font:

```ini
[Font]
Family=Segoe UI
```

The checked-in config keeps the author's `Input` preset. If that font isn't installed, the launcher uses Segoe UI. No fonts are bundled. Input has its own [license](https://input.djr.com/license/); it isn't covered by this project's MIT license.

Font replacement affects the whole interface. Wider fonts can make BPM values or other fixed-width fields show an ellipsis. The palette also affects some artwork and waveform images.

In `[Palette]`, set `TintColor` to any RGB hex color (`#RRGGBB`) and `TintStrength` to its strength from 0 to 100. This adds an overall tint to the processed interface image, including artwork; it isn't limited to blue highlights. For example, `TintColor=#C88B55` with `TintStrength=5` adds a subtle warm tint. Use 0 for no overlay; 100 fills the processed image with the chosen color. Separately rendered OpenGL content and native window elements are not covered by this image filter. Restart after changing settings.

To change the highlight color, set `HighlightColor` to any RGB hex color and `HighlightStrength` to 0–100. For example, `HighlightColor=#B06C42` and `HighlightStrength=100` replace the original blue with orange-brown. Leave `HighlightColor` empty to keep the current colors. This matches pixel colors rather than widget roles, so similar colors in images can change too.

`HighlightSourceColor` optionally selects a different original color to replace; leave it empty to match rekordbox's native blue shades. `BlueDimming` remains available for the old blue-softening preset. The overall tint is applied after these adjustments. Restart to apply changes.

Tested with rekordbox **7.2.18 and 7.2.19**, Windows x64. The newest installed version is selected automatically. Future versions may need changes to the hooks. The binaries are unsigned.

```text
rekordbox-font.exe --list
rekordbox-font.exe --exe "D:\rekordbox\rekordbox.exe"
```

## Build

Install Visual Studio 2022 or newer with **Desktop development with C++** and a Windows SDK. Python 3 is needed only for the discovery test.

```bat
scripts\build.cmd
scripts\test.cmd
powershell -ExecutionPolicy Bypass -File scripts\package.ps1 -Version 0.1.0
```

The EXE, DLL and local config live in `bin/`. Release ZIPs go to `dist/`. The build leaves an existing `bin/font.ini` alone. Tagged GitHub builds also create a release automatically.

[Settings and implementation](docs/technical.md) · [Changelog](CHANGELOG.md)

MIT. MinHook and its disassembler retain their BSD licenses; see [third-party notices](THIRD_PARTY_NOTICES.md). This is an unofficial project, not affiliated with AlphaTheta.
