# Pretentious Name 插件源码

这是来自 https://www.pretentiousname.com/pnop_source/ 的 Directory Opus 插件源码，这些是学习 Opus 插件开发的最佳参考资料。

## 源码下载

如果自动下载失败，请手动从以下链接下载：

### 主要插件包
1. **PNOpusPlugins_2009-08-03.zip**
   - 包含：Audio Tags, GIFAnim, NFO, Targa, TextThumb
   - 下载地址：https://www.pretentiousname.com/pnop_source/PNOpusPlugins_2009-08-03.zip

2. **PNOpusPlugins_drawrap_2016-06-07.7z**
   - 包含：Raw Digital Camera
   - 下载地址：https://www.pretentiousname.com/pnop_source/PNOpusPlugins_drawrap_2016-06-07.7z

3. **maya_iff_1006_source.zip**
   - 包含：Maya IFF
   - 下载地址：https://www.pretentiousname.com/pnop_source/maya_iff_1006_source.zip

### 已过时的插件（仅供参考）
- **dopus8_jp2raw_1100_source.zip** - JP2Raw（已过时）
- **dopus8_ogg_1006_source.zip** - Ogg/FLAC（已过时，被 Audio Tags 替代）

## 插件说明

### Audio Tags
**功能**：音频文件信息 + 缩略图插件  
**支持格式**：iTunes/AAC/M4A/M4P, Apple Lossless (ALAC), Ogg Vorbis, FLAC, Monkey's Audio (APE), Speex  
**学习要点**：
- 安全的 UTF-8 到 UTF-16 转换
- 流支持（可处理归档内的文件）
- 直接 I/O 方式处理流
- 缩略图返回 JPEG/PNG 内存块
- USB 模式、Unicode、x64 支持

### Raw Digital Camera
**功能**：RAW 相机图像查看器 + 缩略图插件  
**基于**：Dave Coffin 的 DCRaw  
**学习要点**：
- 配置对话框
- 流支持
- 将 Unix 命令行程序改造为线程安全的 Windows DLL
- USB 模式、Unicode、x64 支持

### GIFAnim
**功能**：GIF 动画查看器 + 缩略图插件  
**学习要点**：
- 自定义窗口（高级方式）
- 完整的查看器功能（缩放、平铺、旋转、裁剪、打印等）
- 动画支持
- 缩略图 Alpha 通道
- 流支持
- 配置对话框
- 自定义菜单项
- 自定义工具栏
- 通过内容识别文件
- XML 配置文件

### NFO
**功能**：NFO 文本查看器插件  
**学习要点**：
- 自定义窗口（基于子类化的编辑控件）
- 流支持
- 通过扩展名识别文件
- **推荐新手从此插件开始学习！**

### Targa
**功能**：TGA 图像查看器 + 缩略图插件  
**学习要点**：
- 支持 Alpha 通道、预乘 Alpha、索引色、灰度
- 使用模板元编程优化性能
- 流支持
- 与 SDK 示例版本对比学习

### TextThumb
**功能**：文本文件缩略图插件  
**学习要点**：
- 仅缩略图（无查看器）
- 缩略图 Alpha 通道
- 动态重新生成缩略图（不缓存）
- 进程范围的背景图像缓存（带管理线程）
- 流支持
- 配置对话框
- 通过内容识别文件
- XML 配置文件

### Maya IFF
**功能**：Maya IFF 图像查看器 + 缩略图插件  
**许可证**：GPL v2  
**学习要点**：
- Alpha 通道支持
- 流支持
- 文件 IO 抽象（支持内存缓冲区、流、文件名）
- 线程安全的实例数据

## 学习建议

1. **初学者**：先看 NFO 插件，这是最简单的自定义窗口查看器示例
2. **VFS 插件开发**：参考当前项目目录中的 RegVFS、dopus-webdav-vfs、directory_opus_amiga_plugin
3. **查看器插件**：先看 NFO，再看 Targa，最后看 GIFAnim
4. **文件信息/缩略图插件**：看 Audio Tags 和 TextThumb

## 更多信息

请查看上级目录中的 `开发指南.md` 了解更多开发细节。

插件作者：Leo Davidson  
网站：https://www.pretentiousname.com/
