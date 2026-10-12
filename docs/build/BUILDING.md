# 构建与输出位置

以下构建命令均从外层工作区根目录执行；文中的 `out` 位于该根目录。Git 仓库根目录位于内层 `CMakePro_CUDA_SeisSim_V2`，本文件位于仓库的 `docs/build` 中。文档导出命令则从 Git 仓库根目录执行。

## Visual Studio

推荐使用“打开文件夹”打开外层项目目录，并选择 `x64-Debug` 或 `x64-Release`。

如果直接打开内部 `CMakePro_CUDA_SeisSim_V2` 目录，内部新增的 `CMakeSettings.json` 也会把产物放到外层 `out`。配置修改后，等待 VS 完成 CMake 重新配置；若仍显示旧路径，可关闭并重新打开项目文件夹，再选择 CMake 目标 `CMakePro_CUDA_SeisSim_V2.exe` 启动。

## 命令行

在已经初始化 MSVC x64 编译环境的终端中，从外层项目目录执行：

```powershell
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-Debug --target CMakePro_CUDA_SeisSim_V2 --parallel
```

发布配置与当前 VS 的 `x64-Release` 保持一致：

```powershell
cmake -S . -B out/build/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build out/build/x64-Release --target CMakePro_CUDA_SeisSim_V2 --parallel
```

## 目录布局

```text
外层项目/
├─ CMakeLists.txt
├─ CMakeSettings.json
├─ cmake/BuildLayout.cmake
├─ CMakePro_CUDA_SeisSim_V2/       Git 仓库、源码、原始资源与内部入口配置
│  └─ docs/                     Markdown 文档、截图与导出工具
└─ out/
   ├─ build/
   │  ├─ x64-Debug/               外层入口的编译缓存与中间文件
   │  ├─ x64-Release/
   │  └─ standalone/              内部入口的独立缓存
   ├─ bin/
   │  ├─ Debug/
   │  │  ├─ CMakePro_CUDA_SeisSim_V2.exe
   │  │  └─ resource_CUDA_V1/
   │  └─ RelWithDebInfo/
   ├─ lib/                        生成的库文件（如有）
   ├─ install/                    执行安装时的目标位置
   ├─ docs/                       可编辑 Word 说明书
   └─ archive/legacy-2d-data/      清理旧构建时保留的二维导出数据与布局
```

两个入口不能共用一个缓存目录，因为 CMake 缓存记录的源码根目录不同；它们的最终程序输出位置一致。日常建议固定使用外层入口，避免两套构建同时写入同一个程序文件。

当前 `x64-Release` 实际使用 `RelWithDebInfo`，保留了原有配置行为。若单独使用 `-DCMAKE_BUILD_TYPE=Release`，程序将输出到 `out/bin/Release`。

原始字体和着色器仍保留在源码资源目录中，编译后自动同步到程序旁边。运行或分发程序时需要保留同目录下的 `resource_CUDA_V1` 和 `resource_Seis3D`。

三维沙盒的操作、物理格式与适用边界见 [三维实现说明](../development/SEIS3D_IMPLEMENTATION.md)，图文步骤见 [三维使用指南](../user/Seis3D_User_Guide.md)。

VS 自身的 `.vs` 是编辑器工作区缓存，与编译输出目录不同；这里统一的是 CMake 中间文件、程序和部署资源。

## 增量编译的头文件依赖

MSVC + Ninja 的依赖解析必须与 `/showIncludes` 输出的语言和编码一致。曾发现发布配置缓存中的中文前缀被错误解码，Ninja 因而记录零个头文件依赖。修改三维状态结构后，部分渲染目标文件未重编译，运行时便出现只有网格、纹理透明的问题。

`cmake/MsvcDependencies.cmake` 现在通过 `MsvcCompile.cmd` 统一编译器输出为 UTF-8，并用真实编译探针检测已安装语言的包含前缀，同时覆盖旧缓存中的错误值。外层和内部入口、VS 和命令行构建均使用此处理；不依赖是否安装英文语言包。

修复旧缓存后先完整重建一次：

```powershell
cmake --build out/build/x64-Release --clean-first --parallel 2
```

后续正常增量编译即可。可以用 Ninja 的 `-t deps` 检查三维渲染目标是否列出 `Seis3D_Sim.h`、`Common3D.h` 和 `Internal3D.h`。

## 实际程序的三维显示检查

从 `out/bin/RelWithDebInfo` 运行：

```powershell
.\CMakePro_CUDA_SeisSim_V2.exe --check-seis3d-display
```

检查模式使用真实程序启动、96³ 默认模型、CUDA/GL 渲染和 100 道正式采集，在不可见窗口中临时应用四视图布局，检查切片纹理及实际 ImGui 图像提交，并生成 `seis3d-app-display-check.png`。成功输出 `PASS` 并返回 0；此模式不保存或覆盖用户的 ImGui 布局。更完整的交互回归由可选目标 `Seis3DWorkspaceRegression` 提供。

## Paper 风格实际截图

从 `out/bin/RelWithDebInfo` 执行：

```powershell
.\CMakePro_CUDA_SeisSim_V2.exe --capture-seis3d-paper
```

截图入口临时应用 Paper Sand 界面与 Paper scientific 显示预设，使用浅色分层模型、Coolwarm 蓝红波场和浅色体视图。它运行实际 96³ CUDA 模型与 100 道采集，在程序旁输出 `seis3d-paper-home.png`、`seis3d-paper-guide.png` 和 `seis3d-paper-display.png`。此过程不保存用户主题偏好或停靠布局。

本指南的截图副本和局部裁切位于 Git 仓库内的 `docs/assets/seis3d`。更新 Markdown 后，可从 Git 仓库根目录执行 `python docs/tools/export_user_guide.py` 生成 Word，产物位于外层 `out/docs`。Python 环境需要 `python-docx` 与 Pillow；重新导出后应检查分页和截图清晰度。
