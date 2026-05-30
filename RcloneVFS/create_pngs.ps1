Add-Type -AssemblyName System.Drawing

$outDir = "d:\VFS\RcloneVFS"

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

foreach ($sz in @(16, 32, 48)) {
    $bmp = Draw-CloudIcon $sz
    $pngPath = Join-Path $outDir "rclone_$sz.png"
    $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $fi = Get-Item $pngPath
    Write-Host "Created $pngPath ($($fi.Length) bytes)"
}
