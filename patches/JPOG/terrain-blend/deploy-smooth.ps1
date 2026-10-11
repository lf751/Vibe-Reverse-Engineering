param([Parameter(Mandatory = $true)][string]$GameDirectory, [string]$PresetSource, [string]$PresetTarget)
$ErrorActionPreference = 'Stop'
$runtimeSource = 'D:\Unity\dxvk-remix_numos\_output'
$gameRoot = [IO.Path]::GetFullPath($GameDirectory)
$allowed = @('D:\Games\JPOGRTX\JPOG', 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\test-artifacts\JPOG\terrain-blend\test-game')
if ($gameRoot -notin $allowed) { throw 'Unexpected game target.' }
if (Get-CimInstance Win32_Process -Filter "Name='simjp.exe'" | Where-Object { $_.ExecutablePath -eq (Join-Path $gameRoot 'simjp.exe') }) { throw 'Target game is running.' }
$builtDll = 'D:\Unity\dxvk-remix_numos\_Comp64Release\src\d3d9\d3d9.dll'
if ((Get-FileHash $builtDll).Hash -ne (Get-FileHash (Join-Path $runtimeSource 'd3d9.dll')).Hash) { throw 'Package is not the release build.' }
$backupRoot = Join-Path 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\backups\JPOG' ((Get-Date -Format 'yyyy-MM-dd_HHmmss') + '_smooth-deploy-' + (Split-Path $gameRoot -Leaf))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
$target = Join-Path $gameRoot '.trex'
Copy-Item -LiteralPath $target -Destination (Join-Path $backupRoot '.trex') -Recurse
$retainedFiles = @('d3d9.dll', 'd3d9_remix.dll', 'jpog_view_fix.asi', 'proxy.ini', 'rtx.conf', 'bridge.conf', '.trex\NvRemixBridge.exe')
$retained = @{}
foreach ($file in $retainedFiles) {
  $path = Join-Path $gameRoot $file
  if (Test-Path -LiteralPath $path) { $retained[$file] = (Get-FileHash -LiteralPath $path).Hash }
}
if ($PresetTarget) { Copy-Item -LiteralPath $PresetTarget -Destination (Join-Path $backupRoot 'preset.usda') }
Get-ChildItem -LiteralPath $runtimeSource -Force | Copy-Item -Destination $target -Recurse -Force
$manifest = foreach ($file in Get-ChildItem -LiteralPath $runtimeSource -File -Recurse) {
  $relative = $file.FullName.Substring($runtimeSource.Length + 1)
  $destination = Join-Path $target $relative
  $hash = (Get-FileHash -LiteralPath $file.FullName).Hash
  if ($hash -ne (Get-FileHash -LiteralPath $destination).Hash) { throw "Package hash mismatch: $relative" }
  @{ path = $relative; sha256 = $hash }
}
foreach ($file in $retained.Keys) {
  if ($retained[$file] -ne (Get-FileHash -LiteralPath (Join-Path $gameRoot $file)).Hash) { throw "Retained integration changed: $file" }
}
if ($PresetSource) { Copy-Item -LiteralPath $PresetSource -Destination $PresetTarget -Force }
@{ target = $gameRoot; backup = $backupRoot; files = @($manifest); retained = $retained; preset = $PresetTarget } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $PSScriptRoot ('smooth-deployment-' + (Split-Path $gameRoot -Leaf) + '.json'))
Write-Output "Deployed and hash verified $(@($manifest).Count) files. Backup: $backupRoot"


