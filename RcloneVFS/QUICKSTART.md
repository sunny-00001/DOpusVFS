# RcloneVFS 快速使用指南

## 📦 已编译完成

✅ **RcloneVFS.dll** 已成功编译
- 位置: `d:\VFS\RcloneVFS\build\Release\RcloneVFS.dll`
- 大小: 320 KB
- 导出函数: 26个VFS API

## 🚀 快速安装

### 方法1: 使用安装脚本（推荐）
```cmd
# 以管理员身份运行
install.bat
```

### 方法2: 手动安装
1. 复制文件:
   ```
   复制 d:\VFS\RcloneVFS\build\Release\RcloneVFS.dll
   到 C:\Program Files\GPSoftware\Directory Opus\VFSPlugins\
   ```

2. 重启 Directory Opus

## 📋 使用前准备

### 1. 安装 rclone
- 下载: https://rclone.org/downloads/
- 安装后将 rclone.exe 添加到系统 PATH

### 2. 配置云存储
```cmd
# 配置远程存储（如 Google Drive, Dropbox 等）
rclone config

# 查看已配置的远程存储
rclone listremotes
```

## 🎯 使用方法

### 在 Directory Opus 中访问云存储

1. **打开 Directory Opus**

2. **在地址栏输入**: `rclone://`

3. **浏览远程存储**:
   - 双击远程存储名称（如 `gdrive:`）
   - 像浏览本地文件夹一样浏览云存储

### 示例路径

| 路径 | 说明 |
|------|------|
| `rclone://` | 显示所有已配置的远程存储 |
| `rclone://gdrive` | 浏览 Google Drive 根目录 |
| `rclone://gdrive/Documents` | 浏览 Google Drive 的 Documents 文件夹 |
| `rclone://dropbox/Photos` | 浏览 Dropbox 的 Photos 文件夹 |

## ✨ 支持的操作

- ✅ 浏览目录和文件
- ✅ 查看文件信息（大小、日期等）
- ✅ 复制文件（从云端下载）
- ✅ 移动文件
- ✅ 删除文件
- ✅ 重命名文件和文件夹
- ✅ 创建目录
- ✅ 删除目录

## 🔧 配置选项

### 设置 rclone 路径（可选）

如果 rclone 不在系统 PATH 中，可以设置环境变量：
```cmd
setx RCLONE_PATH "C:\path\to\rclone.exe"
```

### 插件配置

在 Directory Opus 中:
1. 设置 → 首选项 → Zip 和其他档案 → 档案和 VFS 插件
2. 找到 "Rclone" 插件
3. 点击"配置"按钮查看插件信息

## ⚠ 注意事项

1. **性能**: 
   - 大文件操作可能需要时间
   - 取决于网络速度和云服务商限制

2. **认证**:
   - 使用 `rclone config` 配置认证
   - 插件本身不处理认证

3. **文件操作**:
   - 文件会先下载到临时目录
   - 大文件可能占用临时空间

## 🐛 故障排除

### 问题: 插件未出现在 Directory Opus 中
**解决方案**:
- 确认 DLL 文件在正确的 VFSPlugins 目录
- 完全重启 Directory Opus
- 检查插件设置中是否已启用

### 问题: 看不到任何远程存储
**解决方案**:
```cmd
# 检查 rclone 是否正常工作
rclone listremotes

# 如果没有配置，运行
rclone config
```

### 问题: "rclone not found" 错误
**解决方案**:
- 确认 rclone.exe 在系统 PATH 中
- 或设置 RCLONE_PATH 环境变量

### 问题: 访问被拒绝
**解决方案**:
- 检查 rclone 配置和认证
- 运行 `rclone config` 重新配置

## 📚 更多信息

详细文档请参阅: [README.md](README.md)

## 🆘 获取帮助

- **rclone 问题**: https://forum.rclone.org/
- **Directory Opus 问题**: https://resource.dopus.com/

---

**版本**: 1.0.0  
**编译日期**: 2026-05-06  
**兼容性**: Directory Opus 12+, rclone v1.50+
