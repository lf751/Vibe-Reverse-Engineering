param([Parameter(Mandatory = $true)][string]$OutputPath)

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class TerrainTestWindow {
  [StructLayout(LayoutKind.Sequential)]
  public struct Rect { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")]
  public static extern bool GetWindowRect(IntPtr window, out Rect rect);
}
'@
$game = Get-Process simjp -ErrorAction Stop
$rect = New-Object TerrainTestWindow+Rect
if (-not [TerrainTestWindow]::GetWindowRect($game.MainWindowHandle, [ref]$rect)) {
  throw 'Cannot obtain JPOG window bounds.'
}
$bitmap = New-Object System.Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
  $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
  $bitmap.Save([IO.Path]::GetFullPath($OutputPath))
} finally {
  $graphics.Dispose()
  $bitmap.Dispose()
}
