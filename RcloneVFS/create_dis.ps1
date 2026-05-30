Add-Type -AssemblyName System.Drawing

$outDir = "d:\VFS\RcloneVFS"
$disPath = Join-Path $outDir "RcloneVFS.dis"

function Draw-CloudIcon([int]$sz) {
    $bmp = New-Object System.Drawing.Bitmap($sz, $sz, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)
    $cloudColor = [System.Drawing.Color]::FromArgb(60, 120, 200)
    $darkColor = [System.Drawing.Color]::FromArgb(40, 80, 160)
    $pen = New-Object System.Drawing.Pen($darkColor, [Math]::Max(1.0, $sz / 28.0))
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $cloudBrush = New-Object System.Drawing.SolidBrush($cloudColor)
    $arrowBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $cx = $sz / 2.0; $cy = $sz * 0.38; $cw = $sz * 0.72; $ch = $sz * 0.28
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

$tempDir = Join-Path $outDir "dis_temp"
if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
New-Item -ItemType Directory -Path $tempDir | Out-Null

$sizes = @(16, 22, 32, 48, 64)
foreach ($sz in $sizes) {
    $bmp = Draw-CloudIcon $sz
    $bmp.Save((Join-Path $tempDir "rclone_$sz.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

$xmlContent = @"
<?xml version="1.0" encoding="UTF-8"?>
<iconset name="RcloneVFS">
    <display_name>Rclone VFS Icons</display_name>
    <copyright>(c) 2026</copyright>
    <artist>RcloneVFS</artist>
    <set size="small" width="16" height="16" filename="rclone_16.png">
        <icon name="rclone" row="1" col="1" />
    </set>
    <set size="small" width="22" height="22" filename="rclone_22.png">
        <icon name="rclone" row="1" col="1" />
    </set>
    <set size="large" width="32" height="32" filename="rclone_32.png">
        <icon name="rclone" row="1" col="1" />
    </set>
    <set size="large" width="48" height="48" filename="rclone_48.png">
        <icon name="rclone" row="1" col="1" />
    </set>
    <set size="large" width="64" height="64" filename="rclone_64.png">
        <icon name="rclone" row="1" col="1" />
    </set>
</iconset>
"@
$xmlContent | Out-File -FilePath (Join-Path $tempDir "Iconset.xml") -Encoding UTF8

if (Test-Path $disPath) { Remove-Item $disPath -Force }

Add-Type -AssemblyName System.IO.Compression
$fs = [System.IO.File]::Create($disPath)
$zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)

foreach ($sz in $sizes) {
    $pngPath = Join-Path $tempDir "rclone_$sz.png"
    $entry = $zip.CreateEntry("rclone_$sz.png", [System.IO.Compression.CompressionLevel]::Optimal)
    $writer = $entry.Open()
    $reader = [System.IO.File]::OpenRead($pngPath)
    $reader.CopyTo($writer)
    $reader.Close()
    $writer.Close()
}

$xmlPath = Join-Path $tempDir "Iconset.xml"
$xmlEntry = $zip.CreateEntry("Iconset.xml", [System.IO.Compression.CompressionLevel]::Optimal)
$xmlWriter = $xmlEntry.Open()
$xmlReader = [System.IO.File]::OpenRead($xmlPath)
$xmlReader.CopyTo($xmlWriter)
$xmlReader.Close()
$xmlWriter.Close()

$zip.Dispose()
$fs.Close()

Remove-Item $tempDir -Recurse -Force

$fi = Get-Item $disPath
Write-Host "Dis file created: $($fi.FullName) ($($fi.Length) bytes)"
Write-Host ""
Write-Host "Installation:"
Write-Host "1. Copy RcloneVFS.dis to:"
Write-Host "   C:\Users\Administrator\AppData\Roaming\GPSoftware\Directory Opus\Icons\"
Write-Host "2. Restart DOpus"
Write-Host "3. In DOpus Preferences, the icon 'rclone' from set 'RcloneVFS' can be used"
