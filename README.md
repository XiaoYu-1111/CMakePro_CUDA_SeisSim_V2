# CMakePro CUDA SeisSim V2

基于 CUDA、OpenGL 4.3、Dear ImGui Docking 和 ImPlot 的二维与三维弹性波模拟工作区。首页分别进入二维波场沙盒与 Seis3D 三维沙盒，切换模式时释放原模式的 GPU 和图形资源。

本目录是 Git 仓库根目录。源码、原始运行资源、Markdown 文档、说明书截图与文档导出工具集中在本仓库，VS 的 Git 更改窗口可以识别这些文件。

## 文档入口

- [三维模拟图文使用说明](docs/user/Seis3D_User_Guide.md)：Paper Sand 界面、浅色模型、蓝红波场和 100 道采集。
- [文档导航](docs/README.md)：操作、构建、实现与历史审查资料。
- [构建与输出位置](docs/build/BUILDING.md)：VS 与命令行统一使用外层 `out`。
- [三维实现说明](docs/development/SEIS3D_IMPLEMENTATION.md)：求解、显存、I/O 与当前验证边界。
- [原二维 README](docs/history/Seis2D_Original_README.md)：保留旧版本介绍、界面与当时的性能描述。

可编辑 Word 由图文指南和截图导出，保存在外层 `out/docs/Seis3D_User_Guide.docx`。导出方式见文档导航。

## 启动与显示

发布程序位于父目录下的 `out/bin/RelWithDebInfo/CMakePro_CUDA_SeisSim_V2.exe`，Debug 位于 `out/bin/Debug`。运行时保留程序旁的 `resource_CUDA_V1` 和 `resource_Seis3D`。

在三维页选择 **Interface theme → Paper Sand** 与 **Visual preset → Paper scientific**，即可使用说明书中的浅色搭配。建议先从 96³ 网格熟悉流程，再逐级增加模型尺寸。

## 目录约定

`docs/user` 保存操作说明，`docs/assets` 保存实际截图，`docs/tools` 保存导出工具；构建说明、实现说明和历史资料也按类别存放于 `docs`。

外层工作区包含共享 CMake 配置。直接在 VS 中打开本仓库时，编译缓存位于外层 `out/build/standalone/<配置>`；从外层入口构建时使用 `out/build/x64-Debug` 或 `out/build/x64-Release`。程序统一输出到外层 `out/bin/<配置>`。
