param([Parameter(Mandatory = $true)][ValidateRange(0, 2)][int]$Mode)
$ErrorActionPreference = 'Stop'
$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$presetPath = Join-Path $workspaceRoot 'test-artifacts\JPOG\terrain-blend\test-game\rtx-remix\mods\terrain-blend-smoke\mod.usda'
$backupPath = Join-Path $workspaceRoot ('backups\JPOG\' + (Get-Date -Format 'yyyy-MM-dd_HHmmss') + '_pom-test-mode')
New-Item -ItemType Directory -Path $backupPath -Force | Out-Null
Copy-Item -LiteralPath $presetPath -Destination (Join-Path $backupPath 'mod.usda')
$contents = Get-Content -LiteralPath $presetPath -Raw
$declaration = "        custom int remix:terrainBlend:displacementMode = $Mode"
if ($contents -match '(?m)^\s*custom int remix:terrainBlend:displacementMode\s*=.*$') {
  $contents = $contents -replace '(?m)^\s*custom int remix:terrainBlend:displacementMode\s*=.*$', $declaration
} else {
  $anchor = '        custom bool remix:terrainBlend:displacementEnabled'
  if (!$contents.Contains($anchor)) { throw 'Terrain displacement declaration is missing.' }
  $contents = $contents.Replace($anchor, $declaration + "`r`n" + $anchor)
}
Set-Content -LiteralPath $presetPath -Value $contents
Write-Output "Isolated terrain displacement mode: $Mode. Backup: $backupPath"
