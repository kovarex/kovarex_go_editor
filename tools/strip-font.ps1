# Rewrites a TrueType font without the tables no renderer reads: VTT's hinting
# sources (TSI0-TSI5), left in by the font's maker, and the digital signature
# (DSIG, which no longer matches once anything changes). Tables stay in tag
# order, 4-byte aligned, with their checksums and head.checkSumAdjustment
# worked out again. It made TitilliumWeb-Regular.ttf 515 KB -> 81 KB:
#   powershell -ExecutionPolicy Bypass -File tools/strip-font.ps1 -In <font.ttf> -Out <font.ttf>
# (The OFL, resources/fonts/OFL.txt, allows it: the font reserves no name.)
param([string]$In, [string]$Out)
$drop = 'TSI0', 'TSI1', 'TSI2', 'TSI3', 'TSI5', 'DSIG'
$b = [IO.File]::ReadAllBytes($In)
function U16($o) { ($b[$o] -shl 8) -bor $b[$o + 1] }
function U32($o) { ([uint32]$b[$o] -shl 24) -bor ([uint32]$b[$o + 1] -shl 16) -bor ([uint32]$b[$o + 2] -shl 8) -bor [uint32]$b[$o + 3] }

$n = U16 4
$tables = @()
for ($i = 0; $i -lt $n; $i++) {
  $r = 12 + 16 * $i
  $tag = [Text.Encoding]::ASCII.GetString($b, $r, 4)
  if ($drop -contains $tag) { continue }
  $off = U32 ($r + 8); $len = U32 ($r + 12)
  $data = New-Object byte[] $len
  [Array]::Copy($b, $off, $data, 0, $len)
  $tables += [pscustomobject]@{ Tag = $tag; Data = $data }
}
$tables = $tables | Sort-Object Tag -CaseSensitive

function Checksum([byte[]]$d) {
  $sum = [uint64]0
  for ($i = 0; $i -lt $d.Length; $i += 4) {
    $v = [uint64]0
    for ($k = 0; $k -lt 4; $k++) { $v = ($v -shl 8) -bor ($(if ($i + $k -lt $d.Length) { $d[$i + $k] } else { 0 })) }
    $sum = ($sum + $v) % 4294967296  # (0xFFFFFFFF would be -1 here)
  }
  [uint32]$sum
}

# head.checkSumAdjustment is 0 while the checksums are worked out.
$head = ($tables | Where-Object Tag -eq 'head').Data
$head[8] = 0; $head[9] = 0; $head[10] = 0; $head[11] = 0

$count = $tables.Count
$pow = 1; $log = 0; while ($pow * 2 -le $count) { $pow *= 2; $log++ }
$ms = New-Object IO.MemoryStream
function W16($v) { $ms.WriteByte(($v -shr 8) -band 255); $ms.WriteByte($v -band 255) }
function W32($v) { W16 (($v -shr 16) -band 0xFFFF); W16 ($v -band 0xFFFF) }
W32 (U32 0); W16 $count; W16 ($pow * 16); W16 $log; W16 ($count * 16 - $pow * 16)
$offset = 12 + 16 * $count
foreach ($t in $tables) {
  $ms.Write([Text.Encoding]::ASCII.GetBytes($t.Tag), 0, 4)
  W32 (Checksum $t.Data); W32 $offset; W32 $t.Data.Length
  $t | Add-Member Offset $offset
  $offset += [Math]::Ceiling($t.Data.Length / 4) * 4
}
foreach ($t in $tables) {
  $ms.Write($t.Data, 0, $t.Data.Length)
  while ($ms.Length % 4) { $ms.WriteByte(0) }
}
$font = $ms.ToArray()
$adjust = [uint32]((2981146554 + 4294967296 - [uint64](Checksum $font)) % 4294967296)  # 0xB1B0AFBA - sum
$h = ($tables | Where-Object Tag -eq 'head').Offset
$font[$h + 8] = ($adjust -shr 24) -band 255; $font[$h + 9] = ($adjust -shr 16) -band 255
$font[$h + 10] = ($adjust -shr 8) -band 255; $font[$h + 11] = $adjust -band 255
[IO.File]::WriteAllBytes($Out, $font)
"{0}: {1} -> {2} bytes, {3} tables" -f (Split-Path $In -Leaf), $b.Length, $font.Length, $count
