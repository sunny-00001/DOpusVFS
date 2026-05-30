Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class IconTest {
    [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr SHGetFileInfoW(
        string pszPath,
        uint dwFileAttributes,
        ref SHFILEINFOW psfi,
        uint cbSizeFileInfo,
        uint uFlags
    );

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

$sfi = New-Object IconTest+SHFILEINFOW

$SHGFI_ICON = 0x100
$SHGFI_SMALLICON = 0x1
$SHGFI_LARGEICON = 0x0
$SHGFI_SYSICONINDEX = 0x4000
$SHGFI_USEFILEATTRIBUTES = 0x10
$FILE_ATTRIBUTE_DIRECTORY = 0x10

Write-Host "=== Test 1: SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON ==="
$sfiL = New-Object IconTest+SHFILEINFOW
$result = [IconTest]::SHGetFileInfoW([string]::Empty, $FILE_ATTRIBUTE_DIRECTORY, [ref]$sfiL, [System.Runtime.InteropServices.Marshal]::SizeOf($sfiL), ($SHGFI_ICON -bor $SHGFI_USEFILEATTRIBUTES -bor $SHGFI_LARGEICON))
Write-Host "Result pointer: $result"
Write-Host "hIcon: $($sfiL.hIcon)"
Write-Host "iIcon: $($sfiL.iIcon)"
Write-Host "szTypeName: $($sfiL.szTypeName)"

Write-Host ""
Write-Host "=== Test 2: SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON ==="
$sfiS = New-Object IconTest+SHFILEINFOW
$result = [IconTest]::SHGetFileInfoW([string]::Empty, $FILE_ATTRIBUTE_DIRECTORY, [ref]$sfiS, [System.Runtime.InteropServices.Marshal]::SizeOf($sfiS), ($SHGFI_ICON -bor $SHGFI_USEFILEATTRIBUTES -bor $SHGFI_SMALLICON))
Write-Host "Result pointer: $result"
Write-Host "hIcon: $($sfiS.hIcon)"
Write-Host "iIcon: $($sfiS.iIcon)"
Write-Host "szTypeName: $($sfiS.szTypeName)"

Write-Host ""
Write-Host "=== Test 3: SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON ==="
$sfiI = New-Object IconTest+SHFILEINFOW
$result = [IconTest]::SHGetFileInfoW([string]::Empty, $FILE_ATTRIBUTE_DIRECTORY, [ref]$sfiI, [System.Runtime.InteropServices.Marshal]::SizeOf($sfiI), ($SHGFI_SYSICONINDEX -bor $SHGFI_USEFILEATTRIBUTES -bor $SHGFI_SMALLICON))
Write-Host "Result pointer: $result"
Write-Host "iIcon (system image list index): $($sfiI.iIcon)"
