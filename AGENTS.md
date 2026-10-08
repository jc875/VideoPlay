# AGENTS.md — VideoPlay 项目 Agent 指南

> 本文件给 AI Agent（含本助手）在本项目中工作时快速恢复上下文用。
> 本项目是「学习参考工程 音视频系统 → 用 libvlc 复刻视频播放器」的学习项目。
> 创建于 2026-10-06，从 Hello World 起步，逐步对照参考工程补功能。

## 项目概述

- 目标：学习并复刻参考工程 `音视频系统` 中的 VideoPlay（基于 libvlc 的控制台视频播放器）。
- 现状：`VideoPlay.cpp` 已是可运行的 libvlc 控制台播放器（2026-10-06 完成：播放股市讨论.mp4、打印音量/时长/分辨率/进度、键盘控制 暂停/续播/停止，运行验证 VLC 窗口出现）；libvlc 头文件/库/dll/plugins 已就位（从参考工程拷贝，字节数与参考一致）。
- 技术栈：C++20、libvlc（VLC SDK）、**Qt 5.15.2（Qt Widgets，32 位）**、VS2026（18.x，PlatformToolset v145）、`.slnx` 解决方案格式（平台 x64 / x86）。

## Qt 客户端（VideoClient）

- 已搭好 **Qt Widgets + libvlc** 客户端工程 `D:\code\VideoPlay\VideoClient\`（2026-10-08 由 MainAgent 初始化，msbuild 编译通过 + 运行验证弹出"视频客户端"窗口）。
- **环境**：
  - Qt 5.15.2 **32 位** MSVC 套件：`D:\Qt\5.15.2\msvc2019`（与现有 32 位 libvlc 匹配；64 位是 `msvc2019_64`，暂不用）。
  - Qt VS Tools 3.5.0（扩展）装在用户级 `C:\Users\金\AppData\Local\Microsoft\VisualStudio\18.0_8605f5fa\Extensions\ahz4r02e.zgi\`（含 QtMSBuild 构建目标）；运行时副本 `C:\Users\金\AppData\Local\QtMsBuild\`。
  - ⚠️ 本机 VS 实为 **VS2026（18.x）**，MSVC 工具集名是 **v145**（14.51/14.52），不是 v143。Qt 5.15.2 是 v142 编译的，v145 链接兼容（ABI 向后兼容）。
- **工程结构**：`VideoClient.vcxproj`（Win32 + v145 + QtInstall 硬编码 msvc2019 + QtModules=core;gui;widgets）+ `main.cpp` + `VideoClientDlg.h/.cpp` + `VideoClientDlg.ui`（空 QMainWindow 画布，用 VS 里 Qt 设计器双击拖控件）+ `VideoClient.qrc`。已加入 `VideoPlay.slnx`。
- **构建**：msbuild 命令行 `MSBuild VideoClient\VideoClient.vcxproj /p:Configuration=Debug /p:Platform=Win32` → exe 落 `VideoClient\Debug\`；Qt 运行库需 `D:\Qt\5.15.2\msvc2019\bin\windeployqt.exe --dir VideoClient\Debug VideoClient\Debug\VideoClient.exe` 部署（Debug 版 Qt5Cored/Guid/Widgetsd + platforms\qwindowsd 等）。
- **关键坑（改这个工程必看）**：
  1. vcxproj 的 `<Keyword>` **必须为 `QtVS_v301`**（Qt VS Tools 3.x 工程格式）；用旧的 `Qt4VSv1.0` 会让 qtvars.xml 不生成，报 `qt_vars.targets` 的 `MSB4044 ReadLinesFromFile File 为空`。
  2. 源文件中文注释会 C4819：已加 `/utf-8` 编译选项，且 `.cpp/.h` 带 UTF-8 BOM。
  3. **不要对仓库跑 `git clean` 删未跟踪文件**——会误删新建的 Qt 源码（本次两次发生，已恢复并提交保护）。
  4. ⚠️ 仓库存在并行工作线（StarUML 设计图重构会话会对本仓库做 `git reset`/`git clean`），多次覆盖 slnx、删除未跟踪源码。**任何 git 写操作前先 `git status`/`reflog` 核验**；若 VideoClient 源码/提交再次丢失，从 reflog 的 `6bc5c2a`（工程）、`718aa9b`（slnx）、`4911c38`（AGENTS.md）恢复。

## 参考工程（只读，别改）

- 解压位置：`C:\Users\金\AppData\Local\Temp\5c15e79d-07b7-4b30-91f5-09f4e09625d1_音视频系统.zip.5d1\音视频系统\音视频系统代码\音视频系统代码\`
- 结构：同一解决方案下三个工程——VideoPlay（libvlc 播放器）、VideoClient、VideoRTSPServer。
- 参考工程平台：**Win32（VS2019 / v142）**，所以它的输出目录不带平台文件夹（见下文"输出目录规则"）。
- ⚠️ 参考 `VideoPlay.cpp` 里写死了绝对路径 `file:///E:\edoyun\study_project\VideoPlay\VideoPlay\股市讨论.mp4`，在本地跑不了；学习时改成相对路径或本机路径。
- ⚠️ zip 里没有打包 exe（发出来前删了），`VideoPlay\Debug\` 里剩的都是中间文件（.obj/.idb/.pdb/.tlog）。

## 关键路径（成品目录已记录）

| 项 | 路径 | 说明 |
|---|---|---|
| 解决方案 | `D:\code\VideoPlay\VideoPlay.slnx` | 平台 x64 / x86 |
| 源码 | `D:\code\VideoPlay\VideoPlay\VideoPlay.cpp` | libvlc 播放器（2026-10-06 已可运行） |
| **成品目录 OutDir（Debug\|Win32，当前路线）** | `D:\code\VideoPlay\Debug\` | VideoPlay.exe + libvlc.dll + libvlccore.dll + plugins\（运行时依赖；32 位库与 Win32 匹配） |
| 成品目录 OutDir（Debug\|x64，暂不用） | `D:\code\VideoPlay\x64\Debug\` | 走 x64 路线时的成品目录（需换 64 位 SDK） |
| 中间目录 IntDir（Debug\|x64） | `D:\code\VideoPlay\VideoPlay\x64\Debug\` | .obj/.pdb/.tlog/.ilk，可删，重编自动再生 |
| libvlc 头文件 | `D:\code\VideoPlay\VideoPlay\include\vlc\` | |
| libvlc 导入库 | `D:\code\VideoPlay\VideoPlay\lib\libvlc.lib`、`libvlccore.lib` | 链接用（Debug\|x64 已在 vcxproj 配好） |
| 测试视频 | `D:\code\VideoPlay\VideoPlay\股市讨论.mp4` | 播放测试素材 |

### 输出目录规则（VS 默认，未手动改过）

出处：VS 安装目录 `MSBuild\Microsoft\VC\v170\Microsoft.Cpp.MSVC.Toolset.Common.props`（OutDir 第 41-42 行，IntDir 第 30-37 行）：

| 平台 | OutDir 输出目录（成品） | IntDir 中间目录（相对工程目录） |
|---|---|---|
| Win32 | `$(SolutionDir)$(Configuration)\` → `Debug\` | `$(Configuration)\` → `工程\Debug\` |
| x64 | `$(SolutionDir)$(Platform)\$(Configuration)\` → `x64\Debug\` | `$(Platform)\$(Configuration)\` → `工程\x64\Debug\` |

- 这就是"参考在 Debug、我在 x64\Debug"的全部原因：**平台不同**，两边的 vcxproj 都没写输出目录。
- 若想让 exe 直接落在 `D:\code\VideoPlay\Debug\`：项目属性 → 常规 → 输出目录改为 `$(SolutionDir)$(Configuration)\`（IntDir 可留默认），改完需重拷 dll/plugins。

## 构建

- 用 VS 打开 `VideoPlay.slnx`，配置选 **Debug | Win32**（当前活动平台；用户选定走 X86 路线，用老师的 32 位 SDK）。
- 链接依赖已配好：`AdditionalDependencies = libvlc.lib;libvlccore.lib`，`AdditionalLibraryDirectories = lib`，`AdditionalIncludeDirectories = .;include;include\vlc;`（**四个组合全部配齐**：Debug/Release × Win32/x64，2026-10-06 补齐）。
- ⚠️ 工程里**没有 PostBuildEvent**，`libvlc.dll / libvlccore.dll / plugins\` 是**手动拷贝**进成品目录的：清了输出目录、改了 OutDir 或切换平台后必须重拷，否则运行时报"找不到 libvlc.dll"。

## 工作流约束（沿用用户其他项目 AGENTS.md 的规矩）

- **改代码前先 `git` 提交当前状态**，再改；改完说明变更点和理由。
  - 已初始化：2026-10-06 `git init -b main`，首个提交 `be8fdff`（身份 jc875 / 34455602@qq.com）。已推送 GitHub：remote `github` = https://github.com/jc875/VideoPlay.git，main 与远端同步。
- **代码必须有注释**：关键逻辑写清理由、假设和不足。
- **不撒谎、不美化**：有问题直接提，不编默认值，不静默降级，失败就说失败。
- **向后兼容**：不破坏已有可运行状态；libvlc 调用先理清对象生命周期（instance/media/player 的 new 与 release 配对，参考工程收尾顺序：player → media → instance）。
- **讲解风格**（用户偏好）：小白视角、概念配生活类比、逐行讲代码、对照"老写法 vs 现代写法"、如实报 bug。
- 改动大时先讲思路和方案，用户同意后再动手。

## 已确认事实（2026-10-06）

1. 参考工程是 Win32 平台：zip 里 `VideoPlay\Debug\` 只有中间文件（vc142 系 = VS2019），exe 默认落解决方案根 `Debug\`（zip 未打包）。
2. 本工程是 x64 平台：exe 在 `x64\Debug\`（OutDir），中间文件在 `VideoPlay\x64\Debug\`（IntDir）——两个 x64 分别是**输出目录**和**中间目录**，都是 VS 默认行为，不是配置差异。
3. `lib` 下还有 `libvlc.la / libvlccore.la`：Linux libtool 生成的文本文件，Windows 链接用不到，可留可删。
4. 参考工程 VideoPlay.cpp 功能骨架：`libvlc_new("--ignore-config")` → `libvlc_media_new_location("file:///...")` → `libvlc_media_player_new_from_media` → play → 轮询等 volume 就绪 → 读 volume/length/宽高 → `_kbhit()` 控制 暂停/续播/停止 → 收尾 release（player → media → instance）。
5. 本工程 `VideoPlay.vcxproj` 四个配置组合（Debug/Release × Win32/x64）的 include/lib 三段已全部配齐（2026-10-06）；`lib` 下 `libvlc.lib/libvlccore.lib` 经 COFF machine 验证为 **32 位（x86）**，只能配 Win32 平台——x64 配置链接它必报 LNK1112（当前 Hello World 未引用 libvlc 符号、链接器惰性加载未触发，属"能编译的假象"）。

## 变更日志

- 2026-10-06：创建本 AGENTS.md；记录成品目录 `D:\code\VideoPlay\x64\Debug\`；查明 Debug vs x64\Debug 差异（平台不同 + VS 默认 OutDir/IntDir 规则，附官方 props 出处）。
- 2026-10-06：`git init -b main` + 首次提交 `be8fdff`（401 文件，含 libvlc SDK 资产；构建产物已 .gitignore）。
- 2026-10-06：推送 GitHub（jc875/VideoPlay），远端 main = 本地 fd516f0。
- 2026-10-06：用户选定走 X86 路线（老师的 32 位 SDK）；补齐 vcxproj 全部 4 个配置组合的三段（附加包含目录/库目录/依赖项），msbuild 验证 Debug|Win32 编译通过（原 C1083 修复）；活动平台改为 Debug|Win32，成品目录为 `D:\code\VideoPlay\Debug\`。
- 2026-10-06：VideoPlay.cpp 改为 libvlc 播放器：路径不再写死 `E:\`，改用 `GetCurrentDirectoryW` + 文件名拼绝对路径（正斜杠 + file:/// 前缀），`Unicode2Utf8` 改为 `cchWideChar=-1` 标准写法；msbuild Debug|Win32 编译通过，运行验证 VLC 视频窗口出现（股市讨论.mp4 正常播放）；dll/plugins 已拷入 `D:\code\VideoPlay\Debug\` 和 `VideoPlay\Debug\` 两处。注意：命令行直接编 vcxproj 时 SolutionDir=项目目录，exe 落在 `VideoPlay\Debug\`；VS 编 .slnx 才落在 `D:\code\VideoPlay\Debug\`。提交 d25f9a5。
- 2026-10-06：桌面复习笔记新增分类 `02_音视频基础\`，第 1 课《VLC 与 libvlc API 入门》总结 VLC 跨平台/插件/三对象生命周期/函数清单/GetCurrentDirectory 坑。
- 2026-10-07：GUI 选型定为 **Qt(Widgets)+libvlc**（用 VS + Qt VS Tools，不换 IDE；MFC 版仅作参考）。设计图对齐参考 PDF 的 MVC 三层：`VideoClient.uml`（StarUML）已补全 Qt 版时序图「Qt视频客户端」= 5 生命线（global→CVideoClientApp→VideoClientController→VideoClientDlg→EVlc）+ 14 消息（main→Init→Create→SetHwnd→connect+QTimer→Play→SetMedia→on_tick→GetPosition→Pause/Stop/SetPosition）。类 guid 与原文件一致；原备份 `VideoClient.uml.bak`（本地、gitignore）。提交 d9ebe90。
