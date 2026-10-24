# Custom launcher for rekordbox

A different font, softer colors and a dark title bar for rekordbox on Windows.

The launcher loads a small DLL into rekordbox. It doesn't patch installed files
or touch your music or database. Start rekordbox normally to use its original look.

## Use

1. Download and extract the Windows x64 ZIP from Releases.
2. Close rekordbox, then run `rekordbox-font.exe` from the extracted folder.
3. To change the font, edit `font.ini` beside the EXE and restart through the launcher.

Release ZIPs use Segoe UI by default. Choose the family name of any installed font:

```ini
[Font]
Family=Segoe UI
```

The checked-in config keeps the author's `Input` preset. If that font isn't
installed, the launcher uses Segoe UI. No fonts are bundled. Input has its own
[license](https://input.djr.com/license/); it isn't covered by this project's MIT license.

Font replacement affects the whole interface. Wider fonts can make BPM values or
other fixed-width fields show an ellipsis. The palette also affects some artwork
and waveform images.

Tested with rekordbox **7.2.18 and 7.2.19**, Windows x64. The newest installed
version is selected automatically. Future versions may need changes to the hooks.
The binaries are unsigned.

```text
rekordbox-font.exe --list
rekordbox-font.exe --exe "D:\rekordbox\rekordbox.exe"
```

## Build

Install Visual Studio 2022 or newer with **Desktop development with C++** and a
Windows SDK. Python 3 is needed only for the discovery test.

```bat
scripts\build.cmd
scripts\test.cmd
powershell -ExecutionPolicy Bypass -File scripts\package.ps1 -Version 0.1.0
```

The EXE, DLL and local config live in `bin/`. Release ZIPs go to `dist/`.
The build leaves an existing `bin/font.ini` alone. Tagged GitHub builds also create
a release automatically.

[Settings and implementation](docs/technical.md) · [Changelog](CHANGELOG.md)

MIT. MinHook and its disassembler retain their BSD licenses; see
[third-party notices](THIRD_PARTY_NOTICES.md). This is an unofficial project,
not affiliated with AlphaTheta.
