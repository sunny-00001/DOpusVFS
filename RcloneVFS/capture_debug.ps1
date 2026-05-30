Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;

public class DebugCapture : IDisposable
{
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateFileMappingA(IntPtr hFile, IntPtr lpFileMappingAttributes, uint flProtect, uint dwMaximumSizeHigh, uint dwMaximumSizeLow, string lpName);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr MapViewOfFile(IntPtr hFileMappingObject, uint dwDesiredAccess, uint dwFileOffsetHigh, uint dwFileOffsetLow, uint dwNumberOfBytesToMap);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool UnmapViewOfFile(IntPtr lpBaseAddress);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateEventA(IntPtr lpEventAttributes, bool bManualReset, bool bInitialState, string lpName);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool SetEvent(IntPtr hEvent);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    const uint PAGE_READWRITE = 0x04;
    const uint FILE_MAP_READ = 0x04;
    const uint WAIT_OBJECT_0 = 0;
    const int BUFFER_SIZE = 4096;

    IntPtr hSharedBuffer;
    IntPtr pSharedBuffer;
    IntPtr hEventBufferReady;
    IntPtr hEventDataReady;

    public void Start(int timeoutMs)
    {
        hSharedBuffer = CreateFileMappingA(IntPtr.Zero, IntPtr.Zero, PAGE_READWRITE, 0, BUFFER_SIZE, "DBWIN_BUFFER");
        if (hSharedBuffer == IntPtr.Zero) { Console.WriteLine("CreateFileMapping failed: " + Marshal.GetLastWin32Error()); return; }

        pSharedBuffer = MapViewOfFile(hSharedBuffer, FILE_MAP_READ, 0, 0, BUFFER_SIZE);
        if (pSharedBuffer == IntPtr.Zero) { Console.WriteLine("MapViewOfFile failed: " + Marshal.GetLastWin32Error()); return; }

        hEventBufferReady = CreateEventA(IntPtr.Zero, false, false, "DBWIN_BUFFER_READY");
        hEventDataReady = CreateEventA(IntPtr.Zero, false, false, "DBWIN_DATA_READY");

        if (hEventBufferReady == IntPtr.Zero || hEventDataReady == IntPtr.Zero) { Console.WriteLine("CreateEvent failed: " + Marshal.GetLastWin32Error()); return; }

        Console.WriteLine("Listening for OutputDebugString messages...");
        DateTime endTime = DateTime.Now.AddMilliseconds(timeoutMs);

        while (DateTime.Now < endTime)
        {
            SetEvent(hEventBufferReady);
            uint waitResult = WaitForSingleObject(hEventDataReady, 500);
            if (waitResult == WAIT_OBJECT_0)
            {
                int pid = Marshal.ReadInt32(pSharedBuffer);
                IntPtr msgPtr = IntPtr.Add(pSharedBuffer, 4);
                string msg = Marshal.PtrToStringAnsi(msgPtr);
                if (msg.Contains("RcloneVFS") || msg.Contains("rclone"))
                {
                    Console.WriteLine("[PID:{0}] {1}", pid, msg.TrimEnd('\n', '\r'));
                }
            }
        }
        Console.WriteLine("Capture timeout reached.");
    }

    public void Dispose()
    {
        if (pSharedBuffer != IntPtr.Zero) UnmapViewOfFile(pSharedBuffer);
        if (hSharedBuffer != IntPtr.Zero) CloseHandle(hSharedBuffer);
        if (hEventBufferReady != IntPtr.Zero) CloseHandle(hEventBufferReady);
        if (hEventDataReady != IntPtr.Zero) CloseHandle(hEventDataReady);
    }
}
"@

$cap = New-Object DebugCapture
try {
    $cap.Start(15000)
} finally {
    $cap.Dispose()
}
