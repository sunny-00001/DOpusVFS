Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Start-Sleep -Milliseconds 500

$screen = [System.Windows.Forms.Screen]::PrimaryScreen
$bounds = $screen.Bounds

$bitmap = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)

$savePath = "C:\dopus_screenshot.png"
$bitmap.Save($savePath, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()

Write-Host "Screenshot saved to $savePath"
Write-Host "File size: $((Get-Item $savePath).Length) bytes"
