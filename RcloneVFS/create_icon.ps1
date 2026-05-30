Add-Type -AssemblyName System.Drawing

$iconPath = "d:\VFS\RcloneVFS\rclone.ico"

function Draw-CloudIcon([int]$sz) {
    $bmp = New-Object System.Drawing.Bitmap($sz, $sz, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)
    
    $cloudColor = [System.Drawing.Color]::FromArgb(60, 120, 200)
    $darkColor = [System.Drawing.Color]::FromArgb(40, 80, 160)
    $whiteColor = [System.Drawing.Color]::White
    
    $pen = New-Object System.Drawing.Pen($darkColor, [Math]::Max(1.0, $sz / 28.0))
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $cloudBrush = New-Object System.Drawing.SolidBrush($cloudColor)
    $arrowBrush = New-Object System.Drawing.SolidBrush($whiteColor)
    
    $cx = $sz / 2.0
    $cy = $sz * 0.38
    $cw = $sz * 0.72
    $ch = $sz * 0.28
    
    $g.FillEllipse($cloudBrush, [float]($cx - $cw/2), [float]($cy - $ch/2), [float]$cw, [float]$ch)
    $lw = $cw * 0.38; $lh = $ch * 1.5
    $g.FillEllipse($cloudBrush, [float]($cx - $cw*0.28), [float]($cy - $lh*0.45), [float]$lw, [float]$lh)
    $rw = $cw * 0.32; $rh = $ch * 1.7
    $g.FillEllipse($cloudBrush, [float]($cx + $cw*0.06), [float]($cy - $rh*0.50), [float]$rw, [float]$rh)
    $g.FillRectangle($cloudBrush, [float]($cx - $cw*0.38), [float]($cy + $ch*0.05), [float]($cw*0.76), [float]($ch*0.45))
    $g.DrawEllipse($pen, [float]($cx - $cw/2), [float]($cy - $ch/2), [float]$cw, [float]$ch)
    
    $ay = $cy + $ch * 0.55; $aw = $sz * 0.11; $ah = $sz * 0.18
    $shaftW = $aw * 0.35; $shaftH = $ah * 0.65
    $g.FillRectangle($arrowBrush, [float]($cx - $shaftW/2), [float]($ay), [float]$shaftW, [float]$shaftH)
    $triPts = @(
        (New-Object System.Drawing.PointF([float]$cx, [float]($ay + $shaftH + $ah*0.35))),
        (New-Object System.Drawing.PointF([float]($cx - $aw), [float]($ay + $shaftH*0.4))),
        (New-Object System.Drawing.PointF([float]($cx + $aw), [float]($ay + $shaftH*0.4)))
    )
    $g.FillPolygon($arrowBrush, $triPts)
    
    $pen.Dispose(); $cloudBrush.Dispose(); $arrowBrush.Dispose(); $g.Dispose()
    return $bmp
}

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$bitmaps = @{}
foreach ($sz in $sizes) { $bitmaps[$sz] = Draw-CloudIcon $sz }

$fs = [System.IO.File]::Create($iconPath)
$bw = New-Object System.IO.BinaryWriter($fs)

$bw.Write([UInt16]0)
$bw.Write([UInt16]1)
$bw.Write([UInt16]$sizes.Count)

$imageData = @{}
foreach ($sz in $sizes) {
    $bmp = $bitmaps[$sz]
    $bmpData = $bmp.LockBits(
        (New-Object System.Drawing.Rectangle(0, 0, $bmp.Width, $bmp.Height)),
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $stride = $bmpData.Stride
    $rawBytes = New-Object byte[] ($stride * $bmp.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($bmpData.Scan0, $rawBytes, 0, $stride * $bmp.Height)
    $bmp.UnlockBits($bmpData)
    
    $xorSize = $bmp.Width * 4 * $bmp.Height
    $andRowSize = [Math]::Ceiling($bmp.Width / 8.0)
    $andPadRowSize = [Math]::Ceiling($andRowSize / 4.0) * 4
    $andSize = $andPadRowSize * $bmp.Height
    
    $xorData = New-Object byte[] $xorSize
    $andData = New-Object byte[] $andSize
    
    for ($y = 0; $y -lt $bmp.Height; $y++) {
        for ($x = 0; $x -lt $bmp.Width; $x++) {
            $srcIdx = ($bmp.Height - 1 - $y) * $stride + $x * 4
            $dstIdx = $y * $bmp.Width * 4 + $x * 4
            
            $b = $rawBytes[$srcIdx]
            $g_ = $rawBytes[$srcIdx + 1]
            $r = $rawBytes[$srcIdx + 2]
            $a = $rawBytes[$srcIdx + 3]
            
            $xorData[$dstIdx + 0] = $b
            $xorData[$dstIdx + 1] = $g_
            $xorData[$dstIdx + 2] = $r
            $xorData[$dstIdx + 3] = $a
            
            if ($a -eq 0) {
                $andByteIdx = $y * $andPadRowSize + [Math]::Floor($x / 8.0)
                $andBitIdx = 7 - ($x % 8)
                $andData[$andByteIdx] = $andData[$andByteIdx] -bor [byte](1 -shl $andBitIdx)
            }
        }
    }
    
    $headerSize = 40
    $planes = 1
    $bpp = 32
    $compression = 0
    $imageSize = $xorSize + $andSize
    $xPPM = 3780
    $yPPM = 3780
    $colorsUsed = 0
    $colorsImportant = 0
    
    $ms = New-Object System.IO.MemoryStream
    $mw = New-Object System.IO.BinaryWriter($ms)
    
    $mw.Write([UInt32]$headerSize)
    $mw.Write([Int32]$bmp.Width)
    $mw.Write([Int32]($bmp.Height * 2))
    $mw.Write([UInt16]$planes)
    $mw.Write([UInt16]$bpp)
    $mw.Write([UInt32]$compression)
    $mw.Write([UInt32]$imageSize)
    $mw.Write([Int32]$xPPM)
    $mw.Write([Int32]$yPPM)
    $mw.Write([UInt32]$colorsUsed)
    $mw.Write([UInt32]$colorsImportant)
    $mw.Write($xorData)
    $mw.Write($andData)
    $mw.Flush()
    
    $imageData[$sz] = $ms.ToArray()
    $mw.Dispose(); $ms.Dispose()
}

$headerSize = 6 + $sizes.Count * 16
$dataOffset = $headerSize

foreach ($sz in $sizes) {
    $data = $imageData[$sz]
    $w = $sz; if ($sz -ge 256) { $w = 0 }
    $bw.Write([byte]$w)
    $bw.Write([byte]$w)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$data.Length)
    $bw.Write([UInt32]$dataOffset)
    $dataOffset += $data.Length
}

foreach ($sz in $sizes) {
    $bw.Write($imageData[$sz])
}

$bw.Flush(); $fs.Close()
foreach ($sz in $sizes) { $bitmaps[$sz].Dispose() }

$fi = Get-Item $iconPath
Write-Host "Icon created: $($fi.Length) bytes with $($sizes.Count) sizes (BI_RGB format)"
