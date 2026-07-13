param(
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$removed = 0
$skippedTracked = 0

function Get-TrackedFiles {
    $set = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $gitRoot = $null
    try {
        $gitRoot = (& git -C $Root rev-parse --show-toplevel 2>$null).Trim()
    } catch {
        return $set
    }

    if ([string]::IsNullOrWhiteSpace($gitRoot)) {
        return $set
    }

    $files = & git -C $Root ls-files --full-name -- . 2>$null
    foreach ($file in $files) {
        if ([string]::IsNullOrWhiteSpace($file)) { continue }
        $full = [System.IO.Path]::GetFullPath((Join-Path $gitRoot ($file -replace '/', [System.IO.Path]::DirectorySeparatorChar)))
        [void]$set.Add($full)
    }
    return $set
}

$tracked = Get-TrackedFiles

function Remove-GeneratedFile {
    param([string]$Path)

    $full = [System.IO.Path]::GetFullPath($Path)
    if ($tracked.Contains($full)) {
        $script:skippedTracked++
        return
    }

    if ($WhatIf) {
        Write-Host "Would remove $full"
    } else {
        Remove-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
    }
    $script:removed++
}

function Remove-GeneratedDirectory {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) { return }

    Get-ChildItem -LiteralPath $Path -Recurse -Force -File | ForEach-Object {
        Remove-GeneratedFile $_.FullName
    }

    if (-not $WhatIf) {
        Get-ChildItem -LiteralPath $Path -Recurse -Force -Directory |
            Sort-Object FullName -Descending |
            ForEach-Object {
                if (-not (Get-ChildItem -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue)) {
                    Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
                }
            }

        if ((Test-Path -LiteralPath $Path) -and -not (Get-ChildItem -LiteralPath $Path -Force -ErrorAction SilentlyContinue)) {
            Remove-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
        }
    }
}

$generatedDirs = @(
    'Debug',
    'Release',
    'Objects',
    'Listings',
    '.launches',
    'MDK-ARM\Debug',
    'MDK-ARM\Release',
    'MDK-ARM\Objects',
    'MDK-ARM\Listings'
)

foreach ($dir in $generatedDirs) {
    Remove-GeneratedDirectory (Join-Path $Root $dir)
}

$generatedFilePatterns = @(
    'build.log',
    '*.log',
    '*.bak',
    '*.tmp',
    '*.axf',
    '*.elf',
    '*.hex',
    '*.bin',
    '*.map',
    '*.lst',
    '*.crf',
    '*.d',
    '*.dep',
    '*.o',
    '*.obj',
    '*.lnp',
    '*.htm',
    '*.scvd',
    'JLinkLog.txt',
    'JLinkSettings.ini'
)

foreach ($pattern in $generatedFilePatterns) {
    Get-ChildItem -LiteralPath $Root -Recurse -Force -File -Filter $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-GeneratedFile $_.FullName
    }
}

Write-Host "CleanGenerated finished. Removed: $removed, skipped tracked files: $skippedTracked"
if ($WhatIf) {
    Write-Host 'Dry run only. Re-run without -WhatIf to remove files.'
}
