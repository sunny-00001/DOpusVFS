# Directory Opus Rclone VFS Plugin

> **Turn your cloud drives into native local disks.**
> A highly optimized Virtual File System (VFS) plugin for Directory Opus, integrating seamlessly with [Rclone](https://rclone.org/).

[![Download Pre-compiled Binary](https://img.shields.io/badge/Download-Pre--compiled%20Binary%20(.dll)-brightgreen?style=for-the-badge)](https://xiumu.gumroad.com/l/dopus-rclone)

🔥 **Supported Cloud Providers:**
Works out-of-the-box with **Google Drive, OneDrive, WebDAV, Amazon S3, Dropbox, FTP/SFTP**, and [over 50+ other cloud storage systems](https://rclone.org/#providers) supported by Rclone.

## Features

- **Native Remote Browsing**: Navigate through your Rclone remotes natively using the standard Directory Opus lister.
- **Full File Management**: Supports standard file operations including listing, copying, moving, renaming, and deleting across clouds and local drives.
- **Smart Editing (Auto-Upload)**: Double-click a cloud file to open it in your favorite editor. The plugin intelligently detects the change and automatically uploads it back to the remote storage the moment you save (`Ctrl+S`).
- **Idle Detection**: Intelligent monitoring lifecycle that cleans up temporary files after a period of inactivity and once the file is no longer locked by other processes.

## Installation

**Option A: The Easy Way (Pre-compiled Binary)**
Don't want to mess with Visual Studio and C++ compilers? You can download the ready-to-use `.dll` file from my Gumroad page. 
👉 **[Download Pre-compiled Binary (.dll) Here](https://xiumu.gumroad.com/l/dopus-rclone)** *(Pay what you want, including $0)*

**Option B: The Developer Way (Build it yourself)**
1. Clone this repository.
2. Open the solution in Visual Studio 2022.
3. Build in `Release` / `x64` mode.
4. Copy the compiled `DOpusRclone.dll` to your Directory Opus `VFSPlugins` directory.
5. Ensure `rclone.exe` is available in the same directory as the DLL or within your system PATH.
6. Restart Directory Opus.

## Usage

You can easily access your Rclone remotes directly from the Directory Opus location bar:
- Type `rclone://` and press Enter to list all your configured Rclone remote drives.
- Type `rclone://<remote>/<path>` (e.g., `rclone://gdrive/Documents`) to directly browse a specific path on a remote drive.

## Important Notes & Known Limitations

### 1. Rclone Process Persistence
The plugin manages an internal Rclone daemon to handle API requests efficiently. 
- **Behavior**: The `rclone` process will not terminate immediately after you finish browsing or editing. 
- **Reason**: It remains active to maintain connection pools and handle background tasks. It will be cleaned up automatically when Directory Opus closes.

### 2. "Open With" vs. Smart Editing
There is a specific behavioral difference between double-clicking a file and using the right-click "Open With" menu.
- **Double-click (Default Open)**: The plugin intercepts the command, extracts the file to a controlled monitoring directory, and tracks changes. **Auto-upload works perfectly here.**
- **Right-click "Open With"**: When using the system's "Open With" context menu, Directory Opus uses its own internal sandbox (`dperm` folders) to present the file to external applications. 
- **Limitation**: Currently, files edited via "Open With" **will not trigger an automatic upload**. To ensure your edits are saved, please use the default "Open" action.

---
*Disclaimer: This is a third-party plugin and is not officially affiliated with GP Software or the Rclone project.*