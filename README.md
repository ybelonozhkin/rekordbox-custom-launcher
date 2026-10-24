# Custom launcher for rekordbox

A different font, softer colors and a dark title bar for rekordbox on Windows.

The launcher loads a small DLL into rekordbox. It doesn't patch installed files or touch your music or database. Start rekordbox normally to use its original look.

The native window title bar also matches a dark Windows theme. This is enabled by default with `DarkTitleBar=1` in the `[Window]` section of `font.ini`; set it to `0` to leave the title bar unchanged.

## Use

1. Download and extract the Windows x64 ZIP from Releases.
2. Close rekordbox, then run `rekordbox-font.exe` from the extracted folder.
3. To change the font, edit `font.ini` beside the EXE and restart through the launcher.

The default font is Segoe UI, included with Windows. You can choose the family name of any installed font:

```ini
[Font]
Family=Segoe UI
```

If the chosen font isn't installed, the launcher falls back to Segoe UI. No fonts are bundled.

Font replacement affects the whole interface. Wider fonts can make BPM values or other fixed-width fields show an ellipsis. The palette also affects some artwork and waveform images.

In `[Palette]`, set `TintColor` to any RGB hex color (`#RRGGBB`) and `TintStrength` to its strength from 0 to 100. This adds an overall tint to the processed interface image, including artwork. For example, `TintColor=#C88B55` with `TintStrength=5` adds a subtle warm tint. Use 0 for no overlay; 100 fills the processed image with the chosen color. Separately rendered OpenGL content and native window elements are not covered by this image filter. Restart after changing settings.

To replace a color, set `HighlightSourceColor` to the original RGB color, `HighlightColor` to the desired RGB color, and `HighlightStrength` to 0–100. Both colors can be any `#RRGGBB` value. For example:

```ini
HighlightSourceColor=#1473E6
HighlightColor=#B06C42
HighlightStrength=100
```

Use 0 strength or leave either color empty to disable replacement. The overall tint is applied afterward. This matches pixel colors rather than widget roles, so similar colors in images can change too. Restart to apply changes.

Tested with rekordbox **7.2.18 and 7.2.19**, Windows x64. The newest installed version is selected automatically. Future versions may need changes to the hooks. The binaries are unsigned.

```text
rekordbox-font.exe --list
rekordbox-font.exe --exe "D:\rekordbox\rekordbox.exe"
```

## Optional: Input Sans Compressed

This is the narrow, light font used during development. The letter shapes weren't manually compressed: Compressed is an existing Input width. The author's download builder combines the chosen styles into a four-style family named `Input`.

1. Open the [official Input download page](https://input.djr.com/download/) and choose **Download a custom four-style family**.
2. Select these styles:

| Family slot | Input style |
| --- | --- |
| Regular | Input Sans Compressed Light |
| Italic | Input Sans Compressed Light Italic |
| Bold | Input Sans Compressed Regular |
| Bold Italic | Input Sans Compressed Italic |

3. Leave the character defaults unchanged and built-in line spacing at **1.2×**. Download and extract the ZIP.
4. Close rekordbox. Select the four `.ttf` files under `Input_Fonts/Input`, right-click and choose **Install** (under **Show more options** on Windows 11).
5. Set the family in `font.ini` and start through the launcher:

```ini
[Font]
Family=Input
```

This maps the family's bold slot to Regular, keeping emphasized text from getting too heavy. Avoid installing several custom Input families at once: the builder gives them the same internal name, `Input`.

Input has a separate [license](https://input.djr.com/license/), not this project's MIT license. Its free license covers development and plain text; non-development use requires a separate license. Download it from the author and check the terms for your use. Font files are not included in this repository or its releases.

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
