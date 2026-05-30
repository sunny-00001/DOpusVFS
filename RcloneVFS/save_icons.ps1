Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;

public class IconSaveTest {
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

    public static void SaveIconToFile(IntPtr hIcon, string filePath) {
        using (Icon icon = Icon.FromHandle(hIcon)) {
            using (Bitmap bmp = icon.ToBitmap()) {
                bmp.Save(filePath);
            }
        }
    }
}
"@ -ReferencedAssemblies System.Drawing

$SHGFI_ICON = 0x100
$SHGFI_LARGEICON = 0x0
$SHGFI_SMALLICON = 0x1
$SHGFI_USEFILEATTRIBUTES = 0x10
$FILE_ATTRIBUTE_DIRECTORY = 0x10

Write-Host "Saving shell32.dll index 3 icons..."
$large = [IntPtr[]]::new(1)
$small = [IntPtr[]]::new(1)
[IconSaveTest]::ExtractIconExW("shell32.dll", 3, $large, $small, 1) | Out-Null
if ($large[0] -ne [IntPtr]::Zero) {
    [IconSaveTest]::SaveIconToFile($large[0], "C:\shell32_idx3_large.png")
    Write-Host "Saved: C:\shell32_idx3_large.png"
    [IconSaveTest]::DestroyIcon($large[0]) | Out-Null
}
if ($small[0] -ne [IntPtr]::Zero) {
    [IconSaveTest]::SaveIconToFile($small[0], "C:\shell32_idx3_small.png")
    Write-Host "Saved: C:\shell32_idx3_small.png"
    [IconSaveTest]::DestroyIcon($small[0]) | Out-Null
}

Write-Host "Saving SHGetFileInfoW folder icons..."
$sfiL = New-Object IconSaveTest+SHFILEINFOW
$result = [IconSaveTest]::SHGetFileInfoW([string]::Empty, $FILE_ATTRIBUTE_DIRECTORY, [ref]$sfiL, [System.Runtime.InteropServices.Marshal]::SizeOf($sfiL), ($SHGFI_ICON -bor $SHGFI_USEFILEATTRIBUTES -bor $SHGFI_LARGEICON))
if ($result -ne [IntPtr]::Zero -and $sfiL.hIcon -ne [IntPtr]::Zero) {
    [IconSaveTest]::SaveIconToFile($sfiL.hIcon, "C:\shgetfileinfo_folder_large.png")
    Write-Host "Saved: C:\shgetfileinfo_folder_large.png"
    [IconSaveTest]::DestroyIcon($sfiL.hIcon) | Out-Null
}

$sfiS = New-Object IconSaveTest+SHFILEINFOW
$result = [IconSaveTest]::SHGetFileInfoW([string]::Empty, $FILE_ATTRIBUTE_DIRECTORY, [ref]$sfiS, [System.Runtime.InteropServices.Marshal]::SizeOf($sfiS), ($SHGFI_ICON -bor $SHGFI_USEFILEATTRIBUTES -bor $SHGFI_SMALLICON))
if ($result -ne [IntPtr]::Zero -and $sfiS.hIcon -ne [IntPtr]::Zero) {
    [IconSaveTest]::SaveIconToFile($sfiS.hIcon, "C:\shgetfileinfo_folder_small.png")
    Write-Host "Saved: C:\shgetfileinfo_folder_small.png"
    [IconSaveTest]::DestroyIcon($sfiS.hIcon) | Out-Null
}

Write-Host "Done!"
