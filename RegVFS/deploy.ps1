Stop-Process -Name "dopus" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "dopusrt" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 3

$src = "d:\VFS\RegistryVFS.dll"
$dst = "D:\Dopus\VFSPlugins\RegistryVFS.dll"

if (Test-Path $src) {
    Copy-Item -Path $src -Destination $dst -Force
    Write-Host "RegistryVFS.dll deployed successfully"
    Start-Sleep -Seconds 1
    Start-Process "D:\Dopus\dopus.exe"
    Write-Host "Directory Opus started"
} else {
    Write-Host "Source file not found: $src"
}