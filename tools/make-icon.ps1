$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$destination = Join-Path $PSScriptRoot '..\resources\windowpin.ico'
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = @()
foreach ($size in $sizes) {
    $bitmap = [Drawing.Bitmap]::new($size, $size)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size / 256.0, $size / 256.0)
    $navy = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 24, 33, 51))
    $blue = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 47, 89, 173))
    $white = [Drawing.Pen]::new([Drawing.Color]::White, 10)
    $muted = [Drawing.Pen]::new([Drawing.Color]::FromArgb(255, 142, 159, 187), 10)
    $graphics.FillRectangle($navy, 8, 8, 240, 240)
    $graphics.DrawRectangle($muted, 46, 76, 130, 132)
    $graphics.FillRectangle($blue, 80, 44, 132, 132)
    $graphics.DrawLine($white, 112, 122, 146, 88)
    $graphics.DrawLine($white, 146, 88, 180, 122)
    $graphics.DrawLine($white, 146, 88, 146, 151)
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
    $images += ,$stream.ToArray()
    $stream.Dispose(); $white.Dispose(); $muted.Dispose(); $navy.Dispose(); $blue.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
$file = [IO.File]::Create($destination)
$writer = [IO.BinaryWriter]::new($file)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($data in $images) { $writer.Write([byte[]]$data) }
$writer.Dispose(); $file.Dispose()
Write-Output "Generated resources/windowpin.ico"
