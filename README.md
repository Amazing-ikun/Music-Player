# MusicPlayer

一个 Windows 本地音乐播放器（Win32 API + C++20）。支持 MP3 / FLAC / WAV，歌单管理与搜索、
播放模式（顺序 / 单曲 / 随机）、倍速、音量平衡、桌面歌词悬浮窗、听歌时长统计等功能。

---

## 一、依赖：BASS 音频库（必须自行下载）

本项目**不包含** BASS 音频库。BASS 是 Un4seen Developments Ltd. 的**专有软件**，
不是开源库，本仓库不转存它的头文件、导入库或 DLL —— 你需要自己去下载。

- 官方下载地址：<https://www.un4seen.com/bass.html>
- 需要的组件：**BASS**（主库）、**BASS_FX**（变速不变调）、**BASSFLAC**（FLAC 解码插件）
- 授权：**非商业用途免费**；商业用途需向 Un4seen 购买授权。本项目是个人非商业项目。
  条款原文见 [`BASS-LICENSE.txt`](BASS-LICENSE.txt)。

### 放置位置

下载并把文件按下面的结构放到项目根目录的 `bass\` 里（这是 CMake 默认查找的位置）：

```
bass/
├── include/
│   ├── bass.h
│   └── bass_fx.h
└── lib/x64/
    ├── bass.dll          bass.lib
    ├── bass_fx.dll       bass_fx.lib
    └── bassflac.dll      bassflac.lib
```

- `include\` 放头文件，`lib\x64\` 放 64 位 DLL 与对应的导入库（`.lib`）。
- BASS 的压缩包里 `C\` 目录对应 `include\`，`X64\`（或 `C\X64\`）对应 `lib\x64\`。
- 放在别处也可以，配置时加 `-DBASS_DIR=<你的目录>` 指定即可。
- 32 位构建则用 `lib\x86\`（并相应调整 `BASS_DIR`）。

> 运行程序时，`bass.dll`、`bass_fx.dll`、`bassflac.dll` 需要和 `MusicPlayer.exe` 在同一目录。

---

## 二、构建

需要 CMake（≥ 3.16）与支持 C++20 的编译器（MinGW-w64 或 MSVC）。

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

产物在 `build\MusicPlayer.exe`。BASS 的三个 DLL 会被自动复制到输出目录。

如未放置 BASS 文件，配置阶段会直接报错并提示需要的路径。

---

## 三、打包安装程序（可选）

1. 先把发布产物暂存到 `dist\`：

   ```bash
   cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
   cmake --build cmake-build-release
   cmake --install cmake-build-release --prefix dist
   ```

2. 用 Inno Setup 编译安装脚本（需自行安装 [Inno Setup](https://jrsoftware.org/isinfo.php)）：

   ```bash
   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\MusicPlayer.iss
   ```

   安装包输出到 `installer-out\`。

安装包**不附带** BASS 的 DLL（原因见上文）。装完后请自行把 `bass.dll`、`bass_fx.dll`、
`bassflac.dll` 复制到安装目录，否则程序无法播放（启动时会给出提示与下载地址）。

---

## 四、运行时数据

程序把自己的状态文件（设置、音量、快捷键、歌单、播放次数、听歌历史、歌词映射、时长与响度缓存）
统一存在**程序目录下的 `Data\`** 里，后缀为 `.mpdf`。日志为程序目录下的 `.error.log`。
升级或迁移时，把整个程序目录（含 `Data\`）一起带走即可保留全部设置与历史。

---

## 五、许可

本项目（即本仓库中的全部自有代码与文档）以
**Apache License 2.0 AND BSD 3-Clause License** 双许可发布，
SPDX 标识：`Apache-2.0 AND BSD-3-Clause`。

"AND"（不是"OR"）表示**两个许可同时适用**，使用者须同时遵守两者的条款；两者要求不一致时，
从严者优先。完整文本见：

- [`LICENSE`](LICENSE) —— 总声明与适用范围
- [`LICENSE-Apache-2.0.txt`](LICENSE-Apache-2.0.txt)
- [`LICENSE-BSD-3-Clause.txt`](LICENSE-BSD-3-Clause.txt)
- [`NOTICE`](NOTICE) —— 归属声明

**第三方组件不在上述许可范围内。** 本仓库不包含 BASS 文件；BASS 由 Un4seen Developments Ltd.
授权，须自行取得，条款见 [`BASS-LICENSE.txt`](BASS-LICENSE.txt)。此外，发布的 Windows 构建
会附带 `libwinpthread-1.dll`（mingw-w64 项目，MIT 式许可），详见 `NOTICE`。
