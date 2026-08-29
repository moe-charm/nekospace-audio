# SPDX-FileCopyrightText: 2026 charmpic
# SPDX-License-Identifier: AGPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('binaural', 'reverb')]
    [string] $Product,

    [Parameter(Mandatory = $true)]
    [string] $Tag,

    [string] $BuildRoot = 'build',
    [string] $OutputRoot = 'dist'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildRoot = Join-Path $repoRoot $BuildRoot
$resolvedOutputRoot = Join-Path $repoRoot $OutputRoot

if ($Product -eq 'binaural') {
    if ($Tag -notmatch '^v(.+)$') { throw "Binaural tag must match v<version>: $Tag" }
    $releaseVersion = $Matches[1]
    $cmakeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt')
    if ($cmakeText -notmatch 'project\(NekoSpaceAudio VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
        throw 'Could not read the Binaural version from CMakeLists.txt'
    }
    $binaryVersion = $Matches[1]
    $archiveBase = "NekoSpaceBinaural-$Tag-Windows-x64"
    $artifactRoot = Join-Path $resolvedBuildRoot 'plugins/binaural/NekoSpaceBinaural_artefacts/Release'
    $copyPlan = @(
        @{ Source = Join-Path $artifactRoot 'VST3/NekoSpace Binaural.vst3'; Name = 'NekoSpace Binaural.vst3'; Recurse = $true },
        @{ Source = Join-Path $artifactRoot 'Standalone/NekoSpace Binaural.exe'; Name = 'NekoSpace Binaural.exe'; Recurse = $false }
    )
    $productReadme = Join-Path $repoRoot 'plugins/binaural/README.md'
    $productChangelog = Join-Path $repoRoot 'CHANGELOG.md'
}
else {
    if ($Tag -notmatch '^reverb-v(.+)$') { throw "Reverb tag must match reverb-v<version>: $Tag" }
    $releaseVersion = $Matches[1]
    $cmakeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'plugins/reverb/CMakeLists.txt')
    if ($cmakeText -notmatch 'set\(NEKOSPACE_REVERB_VERSION "?([0-9]+\.[0-9]+\.[0-9]+)"?\)') {
        throw 'Could not read the Reverb version from plugins/reverb/CMakeLists.txt'
    }
    $binaryVersion = $Matches[1]
    $archiveBase = "NekoSpaceReverb-v$releaseVersion-Windows-x64"
    $artifactRoot = Join-Path $resolvedBuildRoot 'plugins/reverb/NekoSpaceReverb_artefacts/Release'
    $playerRoot = Join-Path $resolvedBuildRoot 'plugins/reverb/NekoSpaceReverbPlayer_artefacts/Release'
    $copyPlan = @(
        @{ Source = Join-Path $artifactRoot 'VST3/NekoSpace Reverb.vst3'; Name = 'NekoSpace Reverb.vst3'; Recurse = $true },
        @{ Source = Join-Path $artifactRoot 'Standalone/NekoSpace Reverb.exe'; Name = 'NekoSpace Reverb.exe'; Recurse = $false },
        @{ Source = Join-Path $playerRoot 'NekoSpace Reverb Player.exe'; Name = 'NekoSpace Reverb Player.exe'; Recurse = $false }
    )
    $productReadme = Join-Path $repoRoot 'plugins/reverb/README.md'
    $productChangelog = Join-Path $repoRoot 'plugins/reverb/CHANGELOG.md'
}

$numericReleaseVersion = ($releaseVersion -split '-', 2)[0]
if ($numericReleaseVersion -ne $binaryVersion) {
    throw "Tag version $numericReleaseVersion does not match $Product binary version $binaryVersion"
}

$packageDirectory = Join-Path $resolvedOutputRoot $archiveBase
$archivePath = "$packageDirectory.zip"
if ((Test-Path -LiteralPath $packageDirectory) -or (Test-Path -LiteralPath $archivePath)) {
    throw "Refusing to overwrite an existing release package: $archiveBase"
}

New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
foreach ($item in $copyPlan) {
    if (-not (Test-Path -LiteralPath $item.Source)) { throw "Missing release artifact: $($item.Source)" }
    Copy-Item -LiteralPath $item.Source -Destination (Join-Path $packageDirectory $item.Name) -Recurse:$item.Recurse
}

Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $packageDirectory
Copy-Item -LiteralPath $productReadme -Destination (Join-Path $packageDirectory 'README.md')
Copy-Item -LiteralPath $productChangelog -Destination (Join-Path $packageDirectory 'CHANGELOG.md')
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/third-party-licenses.md') -Destination $packageDirectory

$commit = (git -C $repoRoot rev-parse HEAD).Trim()
$dirty = [bool] (git -C $repoRoot status --porcelain)
@(
    "product=$Product"
    "tag=$Tag"
    "binary_version=$binaryVersion"
    "git_commit=$commit"
    "git_dirty=$($dirty.ToString().ToLowerInvariant())"
) | Set-Content -LiteralPath (Join-Path $packageDirectory 'BUILD-INFO.txt') -Encoding utf8

Compress-Archive -Path (Join-Path $packageDirectory '*') -DestinationPath $archivePath
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash.ToLowerInvariant()
$checksumPath = "$archivePath.sha256.txt"
"$hash  $(Split-Path -Leaf $archivePath)" | Set-Content -LiteralPath $checksumPath -Encoding ascii
Write-Output $archivePath
Write-Output $checksumPath
