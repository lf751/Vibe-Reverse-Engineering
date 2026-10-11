$ErrorActionPreference = 'Stop'
if (Get-Process simjp -ErrorAction SilentlyContinue) {
    throw 'Close JPOG before restoring its terrain DLL.'
}
$originalDll = 'D:/Unity/Vibe-Reverse-Engineering_JPOG_BetaTools/backups/JPOG/2026-10-09_124036_native-terrain-cache/TTerrainInterface.dll'
$gameDll = 'D:/Games/JPOGRTX/JPOG/TTerrainInterface.dll'
$expectedHash = '4961df57871f539525b95c41dc72001414e4acb23569c8df3c958e155e50e3a3'
if ((Get-FileHash -LiteralPath $originalDll -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'Original terrain DLL backup hash does not match.'
}
Copy-Item -LiteralPath $originalDll -Destination $gameDll
Get-FileHash -LiteralPath $gameDll -Algorithm SHA256
