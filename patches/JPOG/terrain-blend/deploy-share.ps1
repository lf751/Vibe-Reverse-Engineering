$ErrorActionPreference = 'Stop'
$releaseRoot = 'C:\Users\User\Desktop\JPOGLatestRTX'
$runtimeRoot = 'D:\Unity\dxvk-remix_numos\_output'
$bridgeRoot = 'D:\Unity\dxvk-remix_numos\bridge\_output'
$workingGame = 'D:\Games\JPOGRTX\JPOG'
if (Get-CimInstance Win32_Process -Filter "Name='simjp.exe'" | Where-Object { $_.ExecutablePath -eq (Join-Path $releaseRoot 'simjp.exe') }) {
  throw 'Release game is running.'
}
$backupRoot = Join-Path 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\backups\JPOG' ((Get-Date -Format 'yyyy-MM-dd_HHmmss') + '_sharing-release')
New-Item -ItemType Directory -Path $backupRoot | Out-Null
$preserved = @{}
foreach ($file in Get-ChildItem -LiteralPath $releaseRoot -File -Recurse -Force) {
  $relative = $file.FullName.Substring($releaseRoot.Length + 1)
  $preserved[$relative] = (Get-FileHash -LiteralPath $file.FullName).Hash
}
foreach ($name in @('d3d9.dll', 'd3d9_remix.dll', 'jpog_view_fix.asi', 'NvRemixLauncher32.exe', '.trex')) {
  $old = Join-Path $releaseRoot $name
  if (Test-Path -LiteralPath $old) { Copy-Item -LiteralPath $old -Destination $backupRoot -Recurse }
}
$mappings = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $runtimeRoot -File -Recurse) {
  $relative = $file.FullName.Substring($runtimeRoot.Length + 1)
  $mappings['.trex\' + $relative] = $file.FullName
}
$mappings['d3d9.dll'] = 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\patches\JPOG\proxy\d3d9.dll'
$mappings['jpog_view_fix.asi'] = 'D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\patches\JPOG\asi\jpog_view_fix.asi'
$mappings['d3d9_remix.dll'] = Join-Path $bridgeRoot 'd3d9.dll'
$mappings['NvRemixLauncher32.exe'] = Join-Path $bridgeRoot 'NvRemixLauncher32.exe'
$mappings['.trex\NvRemixBridge.exe'] = Join-Path $bridgeRoot '.trex\NvRemixBridge.exe'
$mappings['.trex\bridge.conf'] = Join-Path $workingGame '.trex\bridge.conf'
$mappings['.trex\nvngx_dlssnr.dll'] = Join-Path $workingGame '.trex\nvngx_dlssnr.dll'
foreach ($source in $mappings.Values) {
  if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing dependency: $source" }
}
$builtDll = 'D:\Unity\dxvk-remix_numos\_Comp64Release\src\d3d9\d3d9.dll'
if ((Get-FileHash $builtDll).Hash -ne (Get-FileHash (Join-Path $runtimeRoot 'd3d9.dll')).Hash) { throw 'Runtime package differs from the release build.' }
$manifest = foreach ($relative in $mappings.Keys) {
  $destination = Join-Path $releaseRoot $relative
  New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
  Copy-Item -LiteralPath $mappings[$relative] -Destination $destination -Force
  $hash = (Get-FileHash -LiteralPath $mappings[$relative]).Hash
  if ((Get-FileHash -LiteralPath $destination).Hash -ne $hash) { throw "Deployment mismatch: $relative" }
  @{path = $relative; sha256 = $hash}
}
foreach ($relative in $preserved.Keys) {
  if ($mappings.Contains($relative)) { continue }
  if ((Get-FileHash -LiteralPath (Join-Path $releaseRoot $relative)).Hash -ne $preserved[$relative]) { throw "Authored file changed: $relative" }
}
# Development artifacts are backed up before removal from the sharing folder.
$excluded = @(Get-ChildItem -LiteralPath (Join-Path $releaseRoot '.trex') -File -Recurse | Where-Object { $_.Extension -in @('.pdb', '.lib', '.exp') })
foreach ($file in $excluded) {
  $relative = $file.FullName.Substring($releaseRoot.Length + 1)
  $destination = Join-Path $backupRoot ('excluded\' + $relative)
  New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
  Copy-Item -LiteralPath $file.FullName -Destination $destination
  if (!$file.FullName.StartsWith($releaseRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid removal target.' }
  Remove-Item -LiteralPath $file.FullName
}
$shipping = @($manifest | Where-Object { [IO.Path]::GetExtension($_.path) -notin @('.pdb', '.lib', '.exp') })
@{target = $releaseRoot; backup = $backupRoot; verifiedOverlayFiles = @($manifest).Count; shippingFiles = $shipping; excluded = @($excluded.Name); preservedFiles = $preserved.Count} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'release-share-deployment.json')
Write-Output "Verified $(@($manifest).Count) overlay/integration files; $($shipping.Count) shipping files. Backup: $backupRoot"
