# KeyPlayer

一个基于 C++ 和 Direct2D 的轻量级 Windows 本地音频播放器。
核心音频引擎采用 [Un4seen BASS Library](https://www.un4seen.com/)。

## ✨ 功能特性 (Features)

*   🎧 **高保真播放**：基于 BASS 库，支持 MP3, WAV, FLAC, OGG 等常见格式。
*   🎨 **高性能 UI**：使用 Direct2D 硬件加速绘制界面，丝滑流畅。
*   📦 **便携设计**：所有依赖自包含，无需安装。
*   🖼️ **封面显示**：自动加载并渲染音频封面。

## 🛠️ 开发环境 (Requirements)

*   **OS**: Windows 10 / 11
*   **IDE**: Visual Studio 2022 (推荐)
*   **Language**: C++14 或更高
*   **Dependencies**:
    *   BASS Audio Library (包含在 `vendor/` 目录中)

## 🚀 构建指南 (Build Instructions)

本项目采用源码与构建分离 (Out-of-Source Build) 的方式。

1.  克隆仓库：
    ```bash
    git clone https://github.com/你的用户名/仓库名.git
    cd 仓库名
    ```

2.  使用 Visual Studio 打开根目录下的 `player.sln`。

3.  选择构建配置（Debug 或 Release）和平台（x64）。

4.  点击 **生成 (Build)**。
    *   编译产物会自动生成到 `build/bin/` 目录下。
    *   中间文件会生成到 `build/` 目录下。
    *   `bass.dll` 会自动复制到输出目录。

## 📂 项目结构 (Directory Structure)

```text
/
├── src/           # 源代码 (Source Code)
├── vendor/        # 第三方库 (BASS)
├── resources/     # 编译期资源 (Icon, .rc)
├── build/         # [自动生成] 构建输出目录 (Git Ignored)
├── player.vcxproj.filters
├── player.vcxproj
├── player.sln
└── README.md
```

## 📜 协议与致谢 (License & Credits)

### 💻 代码部分 (Code)
本项目代码采用 MIT 协议开源。

### 🧩 第三方库 (Third-party Libraries)
*   **BASS Audio Library**: Copyright © Un4seen Developments. 
    *   本项目依据 BASS 的非商业许可使用。如果你计划将本项目用于商业用途，请务必前往 [BASS 官网](https://www.un4seen.com/) 购买商业许可证。

### 🎨 图形资源 (Graphics & Assets)
本项目使用了一些优秀的免费图标资源，特此感谢原作者：

*   **应用程序图标 (App Icon)**:
    *   "Multimedia Audio Player Icon" 由 [bokehlicia](https://iconarchive.com/artist/bokehlicia.html) 设计。
    *   来源: [IconArchive](https://www.iconarchive.com/show/alike-icons-by-bokehlicia/multimedia-audio-player-icon.html)
    *   许可: Free for non-commercial use (请确保遵守作者的具体使用条款)

*   **演示封面 (Demo Cover)**:
    *   "Yellow Music CD Icon" 由 [Double-J Design](https://iconarchive.com/artist/double-j-design.html) 设计。
    *   来源: [IconArchive](https://www.iconarchive.com/show/origami-colored-pencil-icons-by-double-j-design/yellow-music-cd-icon.html)
    *   许可: CC Attribution 4.0 (请确保遵守作者的具体使用条款)

