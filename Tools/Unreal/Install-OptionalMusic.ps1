[CmdletBinding()]
param(
    [string]$GameDirectory = '',
    [string]$PackageDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-MusicHash([string]$Path) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-', '') }
    finally { $stream.Dispose(); $algorithm.Dispose() }
}

function Get-ContainedPath([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or
        $Relative -match '[:]' -or ($Relative -split '[/\\]') -contains '..') {
        throw "Invalid package path: $Relative"
    }
    $full = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    if (-not $full.StartsWith($Root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path leaves the music directory: $Relative"
    }
    return $full
}

function Assert-NoLinks([string]$Path) {
    $item = $Path
    while ($item) {
        if (Test-Path -LiteralPath $item) {
            if ((Get-Item -LiteralPath $item -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Choose a regular folder without junctions or symbolic links: $item"
            }
        }
        $parent = [IO.Path]::GetDirectoryName($item.TrimEnd('\'))
        if ($parent -eq $item) { break }
        $item = $parent
    }
}

try {
    if (-not $PackageDirectory) { $PackageDirectory = $PSScriptRoot }
    $packageRoot = [IO.Path]::GetFullPath($PackageDirectory)
    $manifestPath = Join-Path $packageRoot 'music-package.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or @($manifest.files).Count -eq 0) { throw 'Invalid or empty music package.' }
    $relativeRoot = 'SimCopterRemake\Content\Radio\OptionalMusic'
    $sourceRoot = Join-Path $packageRoot $relativeRoot

    if (-not $GameDirectory) {
        if (Test-Path -LiteralPath (Join-Path $packageRoot 'SimCopterRemake.exe')) {
            $GameDirectory = $packageRoot
        } else {
            Add-Type -AssemblyName System.Windows.Forms
            $picker = New-Object System.Windows.Forms.FolderBrowserDialog
            $picker.Description = 'Select your SimCopter Remake folder (containing SimCopterRemake.exe). Requires v1.0.14 or newer.'
            $picker.ShowNewFolderButton = $false
            try {
                if ($picker.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { throw 'Installation cancelled.' }
                $GameDirectory = $picker.SelectedPath
            } finally { $picker.Dispose() }
        }
    }
    $gameRoot = [IO.Path]::GetFullPath($GameDirectory)
    if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'SimCopterRemake.exe') -PathType Leaf)) {
        throw 'Select the extracted game folder containing SimCopterRemake.exe.'
    }
    $buildPath = Join-Path $gameRoot 'BUILD_INFO.json'
    if (-not (Test-Path -LiteralPath $buildPath)) { throw 'Install SimCopter Remake v1.0.14 or newer first (BUILD_INFO.json missing).' }
    $build = Get-Content -LiteralPath $buildPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ([version]$build.version.TrimStart('v') -lt [version]$manifest.minimum_game_version) {
        throw "This music requires SimCopter Remake $($manifest.minimum_game_version) or newer; found $($build.version)."
    }
    $destinationRoot = Join-Path $gameRoot $relativeRoot
    Assert-NoLinks $sourceRoot
    Assert-NoLinks $destinationRoot

    # Validate the whole package before changing anything. Existing different files are
    # deliberately left to the owner; a reinstall or a per-game/bundle overlap is safe.
    $pending = @()
    $seen = @{}
    $skipped = 0
    foreach ($entry in $manifest.files) {
        $relative = [string]$entry.path
        if ($seen.ContainsKey($relative)) { throw "Duplicate manifest path: $relative" }
        $seen[$relative] = $true
        if ($entry.sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw "Invalid checksum: $relative" }
        $source = Get-ContainedPath $sourceRoot $relative
        $destination = Get-ContainedPath $destinationRoot $relative
        Assert-NoLinks $source
        Assert-NoLinks $destination
        if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
            (Get-MusicHash $source) -ne $entry.sha256) {
            throw "Missing or damaged package file: $relative. Extract the download again."
        }
        if (Test-Path -LiteralPath $destination) {
            if (-not (Test-Path -LiteralPath $destination -PathType Leaf) -or
                (Get-MusicHash $destination) -ne $entry.sha256) {
                throw "An existing file differs: $destination. Move it aside before installing this package."
            }
            $skipped++
        } else {
            $pending += [pscustomobject]@{ Source=$source; Destination=$destination; Hash=$entry.sha256 }
        }
    }
    foreach ($entry in $pending) {
        $parent = Split-Path -Parent $entry.Destination
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
        Assert-NoLinks $parent
        $temporary = Join-Path $parent ('.music-install-' + [Guid]::NewGuid().ToString('N') + '.tmp')
        try {
            Copy-Item -LiteralPath $entry.Source -Destination $temporary
            if ((Get-MusicHash $temporary) -ne $entry.Hash) {
                throw "Copied checksum mismatch: $($entry.Destination)"
            }
            Move-Item -LiteralPath $temporary -Destination $entry.Destination
        } finally {
            if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
        }
    }
    Write-Host "Installed $($pending.Count) files; $skipped files were already installed."
    Write-Host "Music folder: $destinationRoot"
    Write-Host 'Restart SimCopter Remake, open Radio, and choose a game collection from the station list.'
    exit 0
} catch {
    [Console]::Error.WriteLine('Installation failed: ' + $_.Exception.Message)
    exit 1
}
