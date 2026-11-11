<img src="resources/windowpin.svg" width="72" alt="WindowPin 图标">

# WindowPin

让重要窗口留在视线里。原生 Windows 窗口置顶工具，支持快捷键、拖动选窗和窗口列表。

[下载最新版本](https://github.com/kuanfuweimu/windowpin/releases/latest) · [English](README.md)

## 开始使用

1. 在 Releases 下载并解压 **WindowPin-1.0.0-win-x64.zip**。
2. 运行 **WindowPin.exe**，无需安装或额外运行时。
3. 切到想置顶的窗口，按 **Ctrl + F8**。再按一次取消置顶。

也可以按住面板里的蓝色选窗区域，拖到目标窗口后松开；或者在列表里选中窗口，点击「置顶 / 取消置顶」。双击列表行也可以切换。拖动过程中按 Esc 取消。

置顶使用 Windows 原生窗口层级，不改变目标窗口的位置、大小，也不会主动把焦点切过去。其他置顶窗口仍可能盖住它。

## 日常操作

| 操作 | 作用 |
| --- | --- |
| Ctrl + F8 | 切换当前活动窗口的置顶状态，可在面板中更换。 |
| 还原所有修改 | 恢复本次会话修改前的置顶状态。 |
| 关闭 / 隐藏 | 隐藏到系统托盘，快捷键继续有效。 |
| 托盘图标 | 单击打开面板；右键可还原或退出。 |
| 退出 | 恢复已记录的窗口状态，注销快捷键并关闭程序。 |
| 登录 Windows 时启动 | 手动启用后生效，默认关闭。 |
| 启动时隐藏到托盘 | 普通启动时不显示主面板。 |
| 语言 | 中文和英文即时切换。 |

列表中的星号 `*` 表示本次会话修改过这个窗口。如果一个窗口原本就处于置顶状态，取消置顶后点击「还原所有修改」，会恢复为置顶。连续切换回原状态后，该窗口会从还原记录中移除。

快捷键支持 Ctrl 或 Alt，可加 Shift，再搭配字母、数字或 F1–F11。快捷键冲突时会提示，之前有效的快捷键会保留。原来的 Window On Top 也可能占用 Ctrl + F8，此时退出原软件或换一个组合。

## 设置与卸载

偏好设置保存在 **EXE 旁边的 WindowPin.ini**，修改选项时才写入。请把文件夹放在可写的位置。软件不联网、不收集统计信息、不安装服务，也不会保存你的窗口标题列表。

只有勾选「登录 Windows 时启动」时，才设置当前用户自己的启动项：

```text
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run\WindowPin
```

启动命令为带引号的 EXE 路径加 `--tray`。移动或卸载前，先取消登录启动，再退出并删除软件文件夹。

## 兼容性

- 支持 Windows 10 1607 及以上、Windows 11，当前发布 x64 版本，C++ 运行库已静态链接。
- 管理员权限运行的应用可能拒绝普通权限的 WindowPin；需要时请手动以相同权限运行。软件不会自动提权。
- 选窗会定位到根所有者窗口，关联对话框遵循 Windows 的层级规则。还原记录针对根窗口，不逐一保存所有附属对话框的历史状态。
- 独占全屏、受保护窗口、安全桌面、不断重设自身层级的应用不保证有效。软件不会高频强制置顶。
- 正常退出和注销时会尝试还原。强制结束、崩溃、断电无法执行清理。若正常退出时还原失败，会提示是否仍然退出。
- 当前发布文件**未进行代码签名**，Windows 可能显示信誉提示；Release 附带 SHA-256 校验文件。

## 编译

需要 Visual Studio 2022 或更新版本的「使用 C++ 的桌面开发」、Windows SDK、CMake 3.24 及以上。在可使用 CMake 的 PowerShell 中运行：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./tools/package.ps1
```

程序位于 `build/Release/WindowPin.exe`，打包产物位于 `dist/`。

## 实现来源

根据 [Skybn Window On Top 的公开功能](https://www.skybn.com/window-always-on-top/)独立重新实现，使用原创代码与图标。与 Skybn 无关联，不包含原程序、反编译实现、原图标、密钥或授权逻辑。采用 MIT 开源许可。

自动检查包含 16 个核心与 Win32 测试，以及中英文、100% / 150% / 200% 缩放、两种宽度共 12 个隐藏界面布局场景。测试只操作自己创建的隐藏窗口，不接管桌面。真实拖动、托盘点击、多显示器切换和第三方应用兼容性仍需人工使用验证，详见 [测试记录](docs/TESTING.md)。
