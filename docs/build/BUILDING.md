# 构建与输出位置

默认构建命令从外层工作区根目录执行；文中的 `out` 位于该根目录。Git 仓库根目录位于内层 `CMakePro_CUDA_SeisSim_V2`，本文件位于仓库的 `docs/build` 中。文档导出命令从 Git 仓库根目录执行；仅克隆内部仓库的构建方式见下节。

## 独立仓库与依赖配置

内部仓库现在包含自己的 `cmake/BuildLayout.cmake`、`MsvcDependencies.cmake` 和 `MsvcCompile.cmd`，不再要求父目录有同名脚本。CMake 最低 3.26；本轮实际使用 VS 附带的 3.31.6-msvc6、MSVC x64 与 CUDA 12.8。主机 C++20、CUDA C++17，Windows/OpenGL 4.3 为当前验证路线。

从内部 Git 仓库根目录，在 MSVC x64 终端执行：

```powershell
cmake -S . -B ../out/build/standalone/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLIB_ROOT=D:/CPP_lib -DSEISSIM_BUILD_SOLVER_TESTS=ON
cmake --build ../out/build/standalone/RelWithDebInfo --target CMakePro_CUDA_SeisSim_V2 Seis2DRecordRegression --parallel 2
```

GLFW/GLEW/GLM 是外部 SDK，默认兼容已有 LIB_ROOT/OpenGL 布局，也可单独指定以下缓存变量。修改 LIB_ROOT 后，如果缓存已经记录旧的单项根目录，应同时更新这些变量或使用新的构建缓存。

| 变量 | 目录必须包含 / 当前 Windows SDK 布局 |
| --- | --- |
| `SEISWAVE_GLM_ROOT` | `glm/glm.hpp` |
| `SEISWAVE_GLEW_ROOT` | `include/GL/glew.h` 和 `lib/Release/x64/glew32s.lib` |
| `SEISWAVE_GLFW_ROOT` | `include/GLFW/glfw3.h` 和 `lib-vc2022/glfw3.lib` |
| `CUDAToolkit_ROOT` | 可选 CUDA 安装选择，由 CMake 查找；编译器可通过 `CMAKE_CUDA_COMPILER` 显式选择 |
| `CMAKE_CUDA_ARCHITECTURES` | 默认 native；分发构建可指定当前工具链支持的架构列表 |

依赖路径可配置不等于自动安装依赖。当前没有在无 CUDA 工具链环境中独立配置纯 CPU 子项目；CPU 记录检查仅指运行时无需 GPU。英文入口见 [README.en](../../README.en.md)，授权待办见 [第三方清单](../../THIRD_PARTY_NOTICES.md)。

## Visual Studio

推荐使用“打开文件夹”打开外层项目目录，并选择 `x64-Debug` 或 `x64-Release`。

如果直接打开内部 `CMakePro_CUDA_SeisSim_V2` 目录，内部新增的 `CMakeSettings.json` 也会把产物放到外层 `out`。配置修改后，等待 VS 完成 CMake 重新配置；若仍显示旧路径，可关闭并重新打开项目文件夹，再选择构建目标 `CMakePro_CUDA_SeisSim_V2` 对应的 `SeisWaveLab.exe` 启动。

软件名称为 SeisWave Lab，三维模块为 Seis3D。当前版本 `2.0.0` 由内层 `CMakeLists.txt` 的 `project(... VERSION ...)` 统一配置，生成的 `AppInfo.h` 和 Windows 文件版本资源位于外层构建目录。程序窗口、主页、“关于”页与 EXE 文件属性使用同一版本信息。工程文件夹与构建目标无需改名。

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
   │  │  ├─ SeisWaveLab.exe
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
.\SeisWaveLab.exe --check-seis3d-display
```

检查模式使用真实程序启动、96³ 默认模型、CUDA/GL 渲染和 100 道正式采集，在不可见窗口中临时应用四视图布局，检查切片纹理及实际 ImGui 图像提交，并生成 `seis3d-app-display-check.png`。成功输出 `PASS` 并返回 0；此模式不保存或覆盖用户的 ImGui 布局。更完整的交互回归由可选目标 `Seis3DWorkspaceRegression` 提供。

六面 CPML 数值对照由默认关闭的 `SEIS3D_BUILD_BOUNDARY_CHECKS` 选项提供，目标为 `Seis3DBoundaryRegression`。尺寸、晚期残留、吸收层色阶和长轴体采样的检查条件、结果及构建方式见 [体绘制与六面吸收检查](../reviews/SEIS3D_VOLUME_BOUNDARY_REVIEW.md)。主程序运行时 Windows 会阻止覆盖其 EXE；编译同一输出配置前应保存计算结果并关闭该程序。

## Paper 风格实际截图

从 `out/bin/RelWithDebInfo` 执行：

```powershell
.\SeisWaveLab.exe --capture-seis3d-paper
```

截图入口临时应用 Paper Sand 界面与 Paper scientific 显示预设，显式恢复该预设默认参数，使用浅色分层模型、Coolwarm 蓝红波场和浅色体视图。它运行实际 96³ CUDA 模型与 100 道采集，在程序旁输出 `seis3d-paper-home.png`、`seis3d-paper-guide.png`、`seis3d-paper-display.png` 和 `seis3d-paper-receiver-row.png`（Receiver focus 布局，第 5 排十道 Vz）。此过程不保存用户主题偏好或停靠布局。

本指南的截图副本和局部裁切位于 Git 仓库内的 `docs/assets/seis3d`。更新 Markdown 后，可从 Git 仓库根目录执行 `python docs/tools/export_user_guide.py` 生成 Word，产物位于外层 `out/docs`。Python 环境需要 `python-docx` 与 Pillow；重新导出后应检查分页和截图清晰度。

英文实际截图使用 `--capture-seis3d-paper`；中文使用 `--capture-seis3d-chinese`，输出 `seis3d-chinese-home.png`、`seis3d-chinese-guide.png`、`seis3d-chinese-display.png`、`seis3d-chinese-receiver-row.png`。两种入口均临时指定语言，不覆盖 `ui_language.ini`。语言资源随 `AppUi` 编译进程序，新增词条修改 `Main_cpp/AppTranslations.inc`，正常增量编译即可。

## 二维记录回归与实际截图

二维实现位于 `src/Seis2D`，私有 `.inc` 只由 `Simulation2D.cpp` 包含一次；公共入口为 `Main_header/Seis_Sim.h`。不要将 `.inc` 当独立源重复编译。

在外层工作区、MSVC x64 终端中启用相关检查：

```powershell
cmake -S . -B out/build/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSEISSIM_BUILD_SOLVER_TESTS=ON -DSEIS3D_BUILD_WORKSPACE_CHECKS=ON
cmake --build out/build/x64-Release --target CMakePro_CUDA_SeisSim_V2 Seis2DRecordRegression SeisSimSolverRegression Seis3DWorkspaceRegression --parallel 2
.\out\bin\RelWithDebInfo\Seis2DRecordRegression.exe
.\out\bin\RelWithDebInfo\SeisSimSolverRegression.exe
.\out\bin\RelWithDebInfo\Seis3DWorkspaceRegression.exe
.\out\bin\RelWithDebInfo\SeisWaveLab.exe --capture-seis2d-paper
python CMakePro_CUDA_SeisSim_V2/docs/tools/prepare_seis2d_screenshots.py
```

记录回归运行时仅使用 CPU，覆盖五种 SEG-Y 样本格式、中文输入/输出路径、时间延迟、失败回滚、AGC/原始数据分离、显示设置保持、数据包碰撞拒绝及导出范围。数值回归和工作区回归需要 CUDA，后者还需要 OpenGL。

二维截图入口使用隐藏的真实应用窗口、256×160 CUDA 双层模型和 100 道正式采集，输出波场、原始多道、AGC 多道、单道 Vz、多道 Vx、停靠对照、记录分析和导出设置八张 1600×1000 PNG。使用临时 Paper Sand 外观，不保存用户主题/布局；语言沿用当前偏好。图片生成后检查局部采样索引、画笔/拾取、采集时缩放/重置、双语窗口 ID、应用源参数与显示状态。成功输出 PASS。截图复制脚本只复制原始像素，无额外图像依赖。

截图入口还在程序旁 `checks/paper-seis2d/session-.../paper_shot_0001` 输出真实 100 道×500 样点的数据包。用 Python 3.11+ 独立只读检查：

```powershell
python CMakePro_CUDA_SeisSim_V2/docs/tools/verify_shot_package.py "完整的数据包目录"
```

启用了 `SEIS3D_BUILD_BOUNDARY_CHECKS` 时，完整构建后再运行 ctest；只编译主程序不会自动重建每个独立回归 EXE，旧测试文件不能作为当前源码的证据。

详细参数见 [二维使用指南](../user/Seis2D_User_Guide.md)，实现和验证范围见 [二维整理说明](../development/SEIS2D_REFACTOR_AND_VIEWER.md)。
