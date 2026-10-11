param([switch]$Baseline)
$ErrorActionPreference = 'Stop'
$gameRoot = 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\test-artifacts\JPOG\terrain-blend\test-game'
$source = Join-Path 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\test-artifacts\JPOG\terrain-blend' 'compressed-runtime'
if ($Baseline) {
  $source = Join-Path (Get-Content (Join-Path $PSScriptRoot 'compression-backup.txt')) 'uncompressed-runtime'
}
if (Get-CimInstance Win32_Process -Filter "Name='simjp.exe'" | Where-Object { $_.ExecutablePath -eq (Join-Path $gameRoot 'simjp.exe') }) {
  throw 'Close the isolated test game before switching runtimes.'
}
if (!(Test-Path -LiteralPath (Join-Path $source 'd3d9.dll'))) { throw 'Runtime package is missing.' }
$backup = Join-Path 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\backups\JPOG' ((Get-Date -Format 'yyyy-MM-dd_HHmmss') + '_compression-test-deploy')
New-Item -ItemType Directory -Path $backup | Out-Null
$target = Join-Path $gameRoot '.trex'
Copy-Item -LiteralPath $target -Destination (Join-Path $backup '.trex') -Recurse
$retained = @{}
foreach ($relative in @('d3d9.dll','d3d9_remix.dll','jpog_view_fix.asi','rtx.conf','user.conf','proxy.ini','.trex\bridge.conf','.trex\NvRemixBridge.exe','rtx-remix\mods\terrain-blend-smoke\mod.usda')) {
  $file = Join-Path $gameRoot $relative
  if (Test-Path -LiteralPath $file) { $retained[$relative] = (Get-FileHash -LiteralPath $file).Hash }
}
Get-ChildItem -LiteralPath $source -Force | Copy-Item -Destination $target -Recurse -Force
$files = foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse) {
  $relative = $file.FullName.Substring($source.Length + 1)
  $hash = (Get-FileHash -LiteralPath $file.FullName).Hash
  if ((Get-FileHash -LiteralPath (Join-Path $target $relative)).Hash -ne $hash) { throw "Runtime mismatch: $relative" }
  @{path = $relative; sha256 = $hash}
}
foreach ($relative in $retained.Keys) {
  if ((Get-FileHash -LiteralPath (Join-Path $gameRoot $relative)).Hash -ne $retained[$relative]) { throw "Integration changed: $relative" }
}
@{baseline = [bool]$Baseline; source = $source; backup = $backup; files = @($files); retained = $retained} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PSScriptRoot 'compression-test-deployment.json')
Write-Output "Verified $(@($files).Count) runtime files. Backup: $backup"
