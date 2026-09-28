# Draws the editor's icon -- a black and a white stone, one over the other --
# at every size Windows asks an icon for, and writes them into
# resources\icon\goeditor.ico. The .ico is checked in; run this again only to
# change the design:
#   powershell -ExecutionPolicy Bypass -File tools\make-icon.ps1
#
# Each size is drawn on its own rather than scaled down from the big one, so
# the stones' edges stay sharp at 16 x 16.

Add-Type -AssemblyName System.Drawing

$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$out   = Join-Path $PSScriptRoot '..\resources\icon\goeditor.ico'

function Color([int]$r, [int]$g, [int]$b, [int]$a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }

# A stone at (cx, cy), `r` its radius: a soft shadow down and to the right, and
# a body lit from the upper left, as the board draws them.
function Stone($g, [float]$cx, [float]$cy, [float]$r, [bool]$black, [int]$size) {
  $shadow = New-Object System.Drawing.SolidBrush (Color 40 22 5 90)
  $off = [Math]::Max(1, $r * 0.12)
  $g.FillEllipse($shadow, $cx - $r + $off, $cy - $r + $off * 1.3, 2 * $r, 2 * $r)

  $path = New-Object System.Drawing.Drawing2D.GraphicsPath
  $path.AddEllipse($cx - $r, $cy - $r, 2 * $r, 2 * $r)
  $brush = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
  $brush.CenterPoint = New-Object System.Drawing.PointF ($cx - $r * 0.35), ($cy - $r * 0.4)
  if ($black) {
    $brush.CenterColor    = Color 118 118 124
    $brush.SurroundColors = @(Color 8 8 10)
  } else {
    $brush.CenterColor    = Color 255 255 255
    $brush.SurroundColors = @(Color 196 194 186)
  }
  $g.FillPath($brush, $path)
  # Small, a white stone needs an edge to stand out from the wood.
  if (-not $black) {
    $edge = New-Object System.Drawing.Pen (Color 120 110 95 ($(if ($size -le 32) { 200 } else { 110 }))), ([Math]::Max(1, $r / 16))
    $g.DrawEllipse($edge, $cx - $r, $cy - $r, 2 * $r, 2 * $r)
  }
}

function Draw([int]$s) {
  $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode   = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.Clear([System.Drawing.Color]::Transparent)

  # Two stones and nothing else: the black one low on the left, the white one
  # high on the right and over it.
  $r = $s * 0.3
  Stone $g ($s * 0.35) ($s * 0.64) $r $true $s
  Stone $g ($s * 0.64) ($s * 0.36) $r $false $s

  $g.Dispose()
  $bmp
}

# An image as an icon stores it, the classic way: a 32-bit bitmap with alpha,
# bottom row first, twice as tall as it is (the second half being the 1-bit
# mask, all clear, as the alpha does its job). Everything reads these; PNG,
# which is only the usual thing at the big sizes, some readers don't.
function IconBitmap($bmp) {
  $s  = $bmp.Width
  $ms = New-Object System.IO.MemoryStream
  $w  = New-Object System.IO.BinaryWriter $ms
  $maskRow = [Math]::Ceiling($s / 32) * 4
  # BITMAPINFOHEADER
  $w.Write([uint32]40); $w.Write([int32]$s); $w.Write([int32](2 * $s))
  $w.Write([uint16]1); $w.Write([uint16]32); $w.Write([uint32]0)
  $w.Write([uint32]($s * $s * 4 + $maskRow * $s))
  $w.Write([int32]0); $w.Write([int32]0); $w.Write([uint32]0); $w.Write([uint32]0)
  for ($y = $s - 1; $y -ge 0; $y--) {
    for ($x = 0; $x -lt $s; $x++) {
      $c = $bmp.GetPixel($x, $y)
      $w.Write([byte]$c.B); $w.Write([byte]$c.G); $w.Write([byte]$c.R); $w.Write([byte]$c.A)
    }
  }
  $w.Write((New-Object byte[] ($maskRow * $s)))
  $w.Flush()
  , $ms.ToArray()  # the comma keeps PowerShell from unrolling it into single bytes
}

# An .ico is a directory of images: bitmaps for the small ones, and PNGs for
# 128 and 256, where a bitmap would be 64 and 256 KB of mostly empty corners
# -- Windows reads PNGs in icons since Vista.
$images = foreach ($s in $sizes) {
  $bmp = Draw $s
  if ($s -ge 128) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $ms.ToArray()
  } else {
    $bytes = IconBitmap $bmp
  }
  $bmp.Dispose()
  , $bytes
}

New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
$file = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter $file
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $s = $sizes[$i]
  $w.Write([byte]($(if ($s -ge 256) { 0 } else { $s }))); $w.Write([byte]($(if ($s -ge 256) { 0 } else { $s })))
  $w.Write([byte]0); $w.Write([byte]0)
  $w.Write([uint16]1); $w.Write([uint16]32)
  $w.Write([uint32]$images[$i].Length); $w.Write([uint32]$offset)
  $offset += $images[$i].Length
}
foreach ($png in $images) { $w.Write($png) }
$w.Flush()
[System.IO.File]::WriteAllBytes((Resolve-Path (Split-Path $out)).Path + '\goeditor.ico', $file.ToArray())

# The biggest one as a PNG too, to look at.
[System.IO.File]::WriteAllBytes((Resolve-Path (Split-Path $out)).Path + '\goeditor-256.png', $images[-1])
Write-Host "Wrote $out"
