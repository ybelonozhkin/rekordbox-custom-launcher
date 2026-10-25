param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.1.0'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$dist = Join-Path $projectRoot 'dist'
[IO.Directory]::CreateDirectory($dist) | Out-Null

# A source archive must describe the exact commit used for the build.
Push-Location $projectRoot
try {
    $changes = @(git status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw 'Initialize and commit the source repository first.' }
    if ($changes.Count) { throw 'Commit the public source files before packaging.' }
    $commit = git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'No source commit found.' }
    $prefix = "custom-launcher-for-rekordbox-$Version"
    $binaryName = "$prefix-windows-x64.zip"
    $sourceName = "$prefix-source.zip"
    $binaryPath = Join-Path $dist $binaryName
    $sourcePath = Join-Path $dist $sourceName
    foreach ($path in @($binaryPath,$sourcePath)) {
        if (Test-Path -LiteralPath $path) { throw "Release already exists: $path" }
    }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $releaseConfig = [IO.File]::ReadAllText((Join-Path $projectRoot 'bin/font.ini'))
    $releaseConfig = [regex]::Replace($releaseConfig, '(?m)^Family=.*$', 'Family=Segoe UI')
    $files = [ordered]@{
        'rekordbox-font.exe' = 'bin/rekordbox-font.exe'
        'font_hook.dll' = 'bin/font_hook.dll'
        'README.md' = 'README.md'
        'assets/screenshot.png' = 'assets/screenshot.png'
        'BUILDING.md' = 'BUILDING.md'
        'CHANGELOG.md' = 'CHANGELOG.md'
        'LICENSE' = 'LICENSE'
        'THIRD_PARTY_NOTICES.md' = 'THIRD_PARTY_NOTICES.md'
        'vendor/minhook/LICENSE.txt' = 'vendor/minhook/LICENSE.txt'
        'docs/technical.md' = 'docs/technical.md'
    }
    foreach ($relative in $files.Values) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $relative) -PathType Leaf)) {
            throw "Missing release file: $relative. Run scripts\build.cmd first."
        }
    }
    $zip = [IO.Compression.ZipFile]::Open($binaryPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entry in $files.GetEnumerator()) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,
                (Join-Path $projectRoot $entry.Value), $entry.Key,
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
        foreach ($textEntry in @{
            'font.ini' = $releaseConfig
            'BUILD.txt' = "Custom launcher for rekordbox $Version`nSource commit: $commit`nPlatform: Windows x64`n"
        }.GetEnumerator()) {
            $writer = [IO.StreamWriter]::new($zip.CreateEntry($textEntry.Key).Open(), [Text.UTF8Encoding]::new($false))
            try { $writer.Write($textEntry.Value) } finally { $writer.Dispose() }
        }
    } finally { $zip.Dispose() }
    git archive --format=zip "--prefix=$prefix/" "--output=$sourcePath" HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
    $checksums = foreach ($path in @($binaryPath,$sourcePath)) {
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $([IO.Path]::GetFileName($path))"
    }
    [IO.File]::WriteAllLines((Join-Path $dist "SHA256SUMS-$Version.txt"), $checksums, [Text.Encoding]::ASCII)
    Write-Output "Release $Version ($commit):"
    Get-ChildItem -LiteralPath $dist -File | Select-Object Name,Length
} finally { Pop-Location }
