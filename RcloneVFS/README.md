# RcloneVFS - Directory Opus VFS Plugin

> **Seamlessly integrate rclone cloud storage into Directory Opus file manager.**

A Virtual File System (VFS) plugin for Directory Opus that provides native support for accessing cloud storage through rclone.

## Features

- **Native Cloud Storage Access**: Browse and manage files on cloud storage providers directly in Directory Opus
- **Multiple Provider Support**: Works with any rclone-supported cloud storage (Google Drive, Dropbox, OneDrive, S3, etc.)
- **Full File Management**: 
  - Browse directories and files
  - Copy, move, and delete files
  - Create and remove directories
  - Rename files and folders
- **Seamless Integration**: Access cloud storage using the `rclone://` prefix in the Directory Opus address bar

## Prerequisites

- **Directory Opus 12** or higher (64-bit)
- **rclone** - Must be installed and accessible
  - Download from: https://rclone.org/downloads/
  - Must be in system PATH or set via `RCLONE_PATH` environment variable
- **Visual Studio 2019** or higher (for building from source)

## Installation

### Option 1: Build from Source

1. **Clone or download this repository**

2. **Open a Visual Studio Developer Command Prompt**
   - From Start Menu: "Developer Command Prompt for VS 2019"
   - Or run: `"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"`

3. **Navigate to the RcloneVFS directory**
   ```cmd
   cd d:\VFS\RcloneVFS
   ```

4. **Build the plugin**
   ```cmd
   build.bat
   ```

5. **Install the plugin**
   - Copy `RcloneVFS.dll` to your Directory Opus VFSPlugins directory:
     ```
     C:\Program Files\GPSoftware\Directory Opus\VFSPlugins\
     ```

6. **Restart Directory Opus**

### Option 2: Manual Compilation

```cmd
cl.exe /MT /O2 /EHsc /utf-8 /LD RcloneVFS.cpp RcloneClient.cpp /I "headers" /link /DEF:RcloneVFS.def /OUT:RcloneVFS.dll
```

## Configuration

### 1. Install and Configure rclone

If you haven't already set up rclone:

```bash
# Install rclone (Windows)
# Download from https://rclone.org/downloads/

# Configure a remote
rclone config
```

Follow the interactive setup to add your cloud storage providers (Google Drive, Dropbox, etc.).

### 2. Set rclone Path (Optional)

If rclone is not in your system PATH, you can set the `RCLONE_PATH` environment variable:

```cmd
setx RCLONE_PATH "C:\path\to\rclone.exe"
```

Or the plugin will automatically search for rclone in:
1. `RCLONE_PATH` environment variable
2. System PATH
3. `C:\Program Files\rclone\rclone.exe`

## Usage

### Accessing Cloud Storage

1. **Open Directory Opus**

2. **Type `rclone://` in the address bar**
   - This will show a list of all configured rclone remotes

3. **Navigate to a remote**
   - Double-click on a remote (e.g., `gdrive:`) to browse its contents
   - Navigate through directories just like local folders

### Example Paths

| Path | Description |
|------|-------------|
| `rclone://` | List all configured remotes |
| `rclone://gdrive` | Browse root of Google Drive remote |
| `rclone://gdrive/Documents` | Browse Documents folder on Google Drive |
| `rclone://dropbox/Photos` | Browse Photos folder on Dropbox |

### Supported Operations

- ✅ Browse directories
- ✅ View file information
- ✅ Copy files (download from cloud)
- ✅ Move files
- ✅ Delete files
- ✅ Rename files and folders
- ✅ Create directories
- ✅ Remove directories

## Project Structure

```
RcloneVFS/
├── headers/
│   ├── vfs plugins.h       # Directory Opus VFS API definitions
│   └── plugin support.h    # Plugin support functions
├── RcloneClient.h          # rclone wrapper class header
├── RcloneClient.cpp        # rclone wrapper implementation
├── RcloneVFS.cpp           # VFS plugin main implementation
├── RcloneVFS.def           # DLL export definitions
├── build.bat               # Build script
└── README.md               # This file
```

## Technical Details

### Architecture

The plugin consists of two main components:

1. **RcloneClient**: Encapsulates rclone command-line operations
   - Executes rclone commands via subprocess
   - Parses JSON output from `rclone lsjson`
   - Handles file operations (copy, move, delete, etc.)

2. **VFS Plugin Interface**: Implements Directory Opus VFS API
   - Directory enumeration
   - File operations
   - Path management
   - Property queries

### API Implementation

The plugin implements the following VFS API functions:

| Function | Purpose |
|----------|---------|
| `VFS_Init` | Initialize plugin and rclone |
| `VFS_Uninit` | Cleanup resources |
| `VFS_IdentifyW` | Return plugin information |
| `VFS_GetPrefixListW` | Return supported path prefix (`rclone://`) |
| `VFS_ReadDirectoryW` | Enumerate directory contents |
| `VFS_GetFileInformationW` | Get file metadata |
| `VFS_CreateFileW` | Open file for read/write |
| `VFS_ReadFile` | Read file contents |
| `VFS_WriteFile` | Write file contents |
| `VFS_SeekFile` | Seek in file |
| `VFS_CloseFile` | Close file handle |
| `VFS_DeleteFileW` | Delete file |
| `VFS_CreateDirectoryW` | Create directory |
| `VFS_RemoveDirectoryW` | Remove directory |
| `VFS_RenameFileW` | Rename file/folder |
| `VFS_MoveFileW` | Move file |
| `VFS_GetPathDisplayNameW` | Get display name for path |
| `VFS_GetPathParentRootW` | Get parent path |
| `VFS_PropGetW` | Query plugin properties |
| `VFS_GetFileSizeW` | Get file size |
| `VFS_Configure` | Show configuration dialog |
| `VFS_About` | Show about dialog |
| `VFS_USBSafe` | USB mode support |

## Troubleshooting

### Plugin not appearing in Directory Opus

1. Ensure `RcloneVFS.dll` is in the correct directory:
   ```
   C:\Program Files\GPSoftware\Directory Opus\VFSPlugins\
   ```

2. Restart Directory Opus completely

3. Check Directory Opus plugin settings:
   - Settings → Preferences → Zip & Other Archives → Archive and VFS Plugins
   - Click "Refresh" to rescan plugins

### Cannot see any remotes

1. Verify rclone is installed and accessible:
   ```cmd
   rclone listremotes
   ```

2. Configure at least one remote:
   ```cmd
   rclone config
   ```

3. Check if rclone is in PATH or set `RCLONE_PATH` environment variable

### Performance issues

- Large directories may take time to load as rclone needs to query the cloud provider
- Consider using rclone's caching features for better performance
- Network speed depends on your internet connection and cloud provider

### Error messages

- **"rclone not found"**: Install rclone or set `RCLONE_PATH` environment variable
- **"Access denied"**: Check your rclone configuration and authentication
- **"Path not found"**: Verify the remote and path exist in your cloud storage

## Limitations

- File operations are synchronous (may block UI for large files)
- No streaming support for large files (files are downloaded to temp location first)
- Requires rclone to be installed separately
- No built-in authentication configuration (use `rclone config`)

## Development

### Building

Requirements:
- Visual Studio 2019 or higher
- Windows SDK
- Directory Opus VFS SDK headers (included)

Build steps:
```cmd
# From Visual Studio Developer Command Prompt
cl.exe /MT /O2 /EHsc /utf-8 /LD RcloneVFS.cpp RcloneClient.cpp /I "headers" /link /DEF:RcloneVFS.def /OUT:RcloneVFS.dll
```

### Debugging

1. Attach Visual Studio debugger to `dopus.exe`
2. Set breakpoints in the plugin code
3. Use `OutputDebugString` for debug logging

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## License

This project is provided as-is for educational and personal use.

## Credits

- **Directory Opus** by GPSoftware - https://www.gpsoft.com.au/
- **rclone** - https://rclone.org/
- Based on Directory Opus VFS Plugin SDK

## Related Projects

- [DOpusWebDAV](../DOpusWebDAV/) - WebDAV VFS plugin for Directory Opus
- [RegVFS](../RegVFS/) - Registry VFS plugin for Directory Opus

## Support

For issues and feature requests, please open an issue in the repository.

For rclone-specific issues, visit: https://forum.rclone.org/

---

**Version**: 1.0.0  
**Last Updated**: 2026-05-06  
**Compatible with**: Directory Opus 12+, rclone v1.50+
