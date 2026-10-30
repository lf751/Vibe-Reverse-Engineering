[CmdletBinding()]
param(
  [string]$TargetRoot = 'C:\Users\User\Desktop\JPOGLatestRTX',
  [string]$RuntimeSourceRoot = 'D:\Unity\dxvk-remix_numos',
  [string]$BackupDirectory,
  [string]$ReportDirectory,
  [string]$LegacyRuntimeSourceRoot = 'D:\Unity\dxvk-remix_numos'
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$releaseRoot = (Resolve-Path -LiteralPath $TargetRoot).Path.TrimEnd('\')
$RuntimeSourceRoot = (Resolve-Path -LiteralPath $RuntimeSourceRoot).Path
$runtimeRoot = Join-Path $RuntimeSourceRoot '_output'
$bridgeRoot = Join-Path $RuntimeSourceRoot 'bridge/_output'
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmmss'
if (!$BackupDirectory) { $BackupDirectory = Join-Path $workspace "backups/JPOG/${stamp}_sharing-release" }
if (!$ReportDirectory) { $ReportDirectory = Join-Path $workspace "test-artifacts/JPOG/plus2-migration/${stamp}_deployment" }
function Assert-Fresh([string]$Binary, [string]$Header) {
  if ((Get-Item -LiteralPath $Binary).LastWriteTimeUtc -lt (Get-Item -LiteralPath $Header).LastWriteTimeUtc) {
    throw "Stale build output: $Binary predates $Header"
  }
}
if (Get-CimInstance Win32_Process -Filter "Name='simjp.exe'" | Where-Object { $_.ExecutablePath -eq (Join-Path $releaseRoot 'simjp.exe') }) {
  throw 'Release game is running.'
}
$backupRoot = $BackupDirectory
if (Test-Path -LiteralPath $backupRoot) { throw "Backup directory already exists: $backupRoot" }
$builtDll = Join-Path $RuntimeSourceRoot '_Comp64Release/src/d3d9/d3d9.dll'
Assert-Fresh $builtDll (Join-Path $RuntimeSourceRoot 'src/dxvk/shaders/rtx/pass/terrain_blend_args.h')
foreach ($name in @('d3d9.dll', '.trex/NvRemixBridge.exe', 'NvRemixLauncher32.exe')) {
  Assert-Fresh (Join-Path $bridgeRoot $name) (Join-Path $RuntimeSourceRoot 'bridge/src/util/util_commands.h')
}
if ((Get-FileHash $builtDll).Hash -ne (Get-FileHash (Join-Path $runtimeRoot 'd3d9.dll')).Hash) { throw 'Runtime package differs from the release build.' }
New-Item -ItemType Directory -Force -Path $backupRoot,$ReportDirectory | Out-Null
$preserved = @{}
foreach ($file in Get-ChildItem -LiteralPath $releaseRoot -File -Recurse -Force) {
  $relative = $file.FullName.Substring($releaseRoot.Length + 1)
  $preserved[$relative] = (Get-FileHash -LiteralPath $file.FullName).Hash
}
foreach ($name in @('d3d9.dll', 'd3d9_remix.dll', 'jpog_view_fix.asi', 'NvRemixLauncher32.exe', '.trex', 'rtx.conf', 'rtx-remix/weather')) {
  $old = Join-Path $releaseRoot $name
  if (Test-Path -LiteralPath $old) {
    $backupDestination = Join-Path $backupRoot $name
    New-Item -ItemType Directory -Force -Path (Split-Path $backupDestination -Parent) | Out-Null
    Copy-Item -LiteralPath $old -Destination $backupDestination -Recurse -Force
    $savedFiles = if (Test-Path -LiteralPath $old -PathType Leaf) { @(Get-Item -LiteralPath $old) } else { @(Get-ChildItem -LiteralPath $old -File -Recurse -Force) }
    foreach ($file in $savedFiles) {
      $relative = $file.FullName.Substring($releaseRoot.Length + 1)
      if ((Get-FileHash -LiteralPath (Join-Path $backupRoot $relative)).Hash -ne $preserved[$relative]) { throw "Recovery backup mismatch: $relative" }
    }
  }
}
$mappings = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $runtimeRoot -File -Recurse -Force) {
  $relative = $file.FullName.Substring($runtimeRoot.Length + 1)
  $mappings['.trex\' + $relative] = $file.FullName
}
$mappings['d3d9.dll'] = Join-Path $workspace 'patches/JPOG/proxy/d3d9.dll'
$mappings['jpog_view_fix.asi'] = Join-Path $workspace 'patches/JPOG/asi/jpog_view_fix.asi'
$mappings['d3d9_remix.dll'] = Join-Path $bridgeRoot 'd3d9.dll'
$mappings['NvRemixLauncher32.exe'] = Join-Path $bridgeRoot 'NvRemixLauncher32.exe'
$mappings['.trex\NvRemixBridge.exe'] = Join-Path $bridgeRoot '.trex\NvRemixBridge.exe'
# Target bridge settings and optional DLLs absent from the package remain authored locally.
if (Test-Path -LiteralPath (Join-Path $releaseRoot '.trex/bridge.conf')) {
  $mappings.Remove('.trex\bridge.conf')
} else {
  $mappings['.trex\bridge.conf'] = Join-Path $workspace 'patches/JPOG/bridge.conf'
}
$config = Join-Path $releaseRoot 'rtx.conf'
$migrated = $false
if (!(Test-Path -LiteralPath $config -PathType Leaf)) { throw 'Target rtx.conf is required for weather migration.' }
$presetDirectory = Select-String -LiteralPath $config -Pattern '^\s*rtx\.weather\.presetDirectory\s*=\s*(.*)$'
foreach ($setting in $presetDirectory) {
  if ($setting.Matches[0].Groups[1].Value.Trim().Trim('"').Replace('\', '/').TrimEnd('/') -ne 'rtx-remix/weather') {
    throw 'Custom rtx.weather.presetDirectory requires an explicit deployment plan; expected rtx-remix/weather.'
  }
}
if ((Select-String -LiteralPath $config -Pattern '^\s*rtx\.weather\.preset\.' -Quiet) -or !(Test-Path -LiteralPath (Join-Path $releaseRoot 'rtx-remix/weather/jpog-clear.conf'))) {
  $converted = Join-Path $ReportDirectory 'converted-profile'
  & python (Join-Path $PSScriptRoot '../plus2-migration/generate_profile.py') --config $config --output $converted --legacy-runtime $LegacyRuntimeSourceRoot --runtime $RuntimeSourceRoot
  if ($LASTEXITCODE -ne 0) { throw 'Weather conversion failed.' }
  $profiles = @(Get-ChildItem -LiteralPath (Join-Path $converted 'weather') -Filter '*.conf' -File)
  if ($profiles.Count -ne 12) { throw "Expected 12 weather profiles, got $($profiles.Count)." }
  $mappings['rtx.conf'] = Join-Path $converted 'rtx.conf'
  foreach ($file in $profiles) { $mappings['rtx-remix\weather\' + $file.Name] = $file.FullName }
  $migrated = $true
}
foreach ($source in $mappings.Values) {
  if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing dependency: $source" }
}
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
  if ((Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) { throw "Backup mismatch: $relative" }
  if (!$file.FullName.StartsWith($releaseRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid removal target.' }
  Remove-Item -LiteralPath $file.FullName
}
$shipping = @($manifest | Where-Object { [IO.Path]::GetExtension($_.path) -notin @('.pdb', '.lib', '.exp') })
@{target = $releaseRoot; runtimeSource = $RuntimeSourceRoot; backup = $backupRoot; weatherMigrated = $migrated; verifiedOverlayFiles = @($manifest).Count; shippingFiles = $shipping; excluded = @($excluded.Name); preservedFiles = $preserved.Count} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'deployment.json')
Write-Output "Verified $(@($manifest).Count) overlay/integration files; $($shipping.Count) shipping files. Backup: $backupRoot"
