Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class ShellIconTest {
    [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
    public static extern uint ExtractIconExW(string szFileName, int nIconIndex, IntPtr[] phiconLarge, IntPtr[] phiconSmall, uint nIcons);

    [DllImport("user32.dll")]
    public static extern bool DestroyIcon(IntPtr hIcon);

    [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr SHGetFileInfoW(string pszPath, uint dwFileAttributes, ref SHFILEINFOW psfi, uint cbSizeFileInfo, uint uFlags);

    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct SHFILEINFOW {
        public IntPtr hIcon;
        public int iIcon;
        public uint dwAttributes;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=260)]
        public string szDisplayName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=80)]
        public string szTypeName;
    }
}
"@

Write-Host "=== Testing ExtractIconExW from shell32.dll ==="
for ($i = 0; $i -le 10; $i++) {
    $large = [IntPtr[]]::new(1)
    $small = [IntPtr[]]::new(1)
    $count = [ShellIconTest]::ExtractIconExW("shell32.dll", $i, $large, $small, 1)
    if ($large[0] -ne [IntPtr]::Zero) {
        Write-Host "Index $i : hLarge=$($large[0]) hSmall=$($small[0]) count=$count"
        [ShellIconTest]::DestroyIcon($large[0]) | Out-Null
        if ($small[0] -ne [IntPtr]::Zero) { [ShellIconTest]::DestroyIcon($small[0]) | Out-Null }
    } else {
        Write-Host "Index $i : no icon"
    }
}

Write-Host ""
Write-Host "=== Testing SHGetFileInfoW for folder icon ==="
$sfi = New-Object ShellIconTest+SHFILEINFOW
$SHGFI_ICON = 0x100
$SHGFI_LARGEICON = 0x0
$SHGFI_SMALLICON = 0x1
$SHGFI_USEFILEATTRIBUTES = 0x10
$FILE_ATTRIBUTE_DIRECTORY = 0x10

$result = [ShellIconTest]::SHGetFileInfoW([string]::Empty, $FILE_ATTRIBUTE_DIRECTORY, [ref]$sfi, [System.Runtime.InteropServices.Marshal]::SizeOf($sfi), ($SHGFI_ICON -bor $SHGFI_USEFILEATTRIBUTES -bor $SHGFI_LARGEICON))
Write-Host "SHGetFileInfoW folder: hIcon=$($sfi.hIcon) iIcon=$($sfi.iIcon) szTypeName=$($sfi.szTypeName)"
