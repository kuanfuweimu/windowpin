param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build'),
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\dist'),
    [string]$Version = '1.0.0',
    [ValidateSet('x64', 'arm64')][string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$binary = Join-Path $BuildDir 'Release\WindowPin.exe'
if (-not (Test-Path -LiteralPath $binary)) { throw 'Build Release before packaging.' }
$stage = Join-Path $OutputDir "WindowPin-$Version-win-$Architecture"
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach ($file in @('README.md', 'README.zh-CN.md', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $repo $file) -Destination $stage
}
Copy-Item -LiteralPath $binary -Destination $stage
foreach ($directory in @('docs', 'resources')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $stage $directory) | Out-Null
}
foreach ($relative in @('docs\TESTING.md', 'docs\IMPLEMENTATION.md', 'resources\windowpin.svg')) {
    Copy-Item -LiteralPath (Join-Path $repo $relative) -Destination (Join-Path $stage $relative)
}
$zip = "$stage.zip"
$payload = @('WindowPin.exe', 'README.md', 'README.zh-CN.md', 'LICENSE', 'docs', 'resources') | ForEach-Object { Join-Path $stage $_ }
Compress-Archive -LiteralPath $payload -DestinationPath $zip -Force
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$zip.sha256", "$hash  $([IO.Path]::GetFileName($zip))`n", [Text.UTF8Encoding]::new($false))
Write-Output $zip
Write-Output "$zip.sha256"
