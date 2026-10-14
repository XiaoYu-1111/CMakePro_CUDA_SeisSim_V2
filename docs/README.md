# SeisWave Lab 文档导航

文档整理日期为 2026 年 10 月 14 日。本目录位于 Git 仓库内部，文档、截图和导出工具可与源码一起提交。按使用模式阅读二维或三维图文说明，再按需要查看构建、算法实现和历史审查。

| 分类 | 文档 | 内容 |
| --- | --- | --- |
| 使用说明 | [二维模拟与地震记录指南](user/Seis2D_User_Guide.md) | 模型到采集、三种停靠布局、单/多道、AGC、增益保持、单炮数据包和八张实际截图 |
| 论文准备 | [投稿与可复现实验路线](publication/PUBLICATION_ROADMAP.md) | 科学验证矩阵、源归一化、性能实验、开源发布与期刊建议 |
| 实现与审查 | [二维整理与查看器升级](development/SEIS2D_REFACTOR_AND_VIEWER.md) | 文件职责、生命周期、采集修复、验证范围、后续优化优先级 |
| 使用说明 | [Seis3D 图文指南](user/Seis3D_User_Guide.md) | 首页、计算参数、切片、浅色样式、默认 150 道与 300 道上限、模型和 SEG-Y 导入导出 |
| 视频教程 | [模型到采集与 AGC 实操](user/Seis3D_Video_Tutorial.md) | OBS 实际操作录像、中文字幕、采样与导出步骤；视频文件在外层 `out/videos` |
| 项目展示 | [CUDA 加速 3D 弹性波模拟](showcase/CUDA_Seis3D_GitHub_Pages.md) | 个人博客文章，含摘要、计算方法、实际截图与采集流程 |
| 可编辑说明书 | Word 导出版本 | 从图文指南生成，保存到外层 `out/docs/Seis3D_User_Guide.docx` |
| 构建 | [BUILDING](build/BUILDING.md) | VS、命令行、外层 out、资源部署和增量编译 |
| 实现 | [SEIS3D_IMPLEMENTATION](development/SEIS3D_IMPLEMENTATION.md) | 三维隔离、差分、CPML、自由表面、显存和文件格式 |
| 审查记录 | [数值稳定性审查](reviews/NUMERICAL_STABILITY_REVIEW.md) | 二维求解器修复与当时的回归结果 |
| 审查记录 | [体绘制与六面吸收检查](reviews/SEIS3D_VOLUME_BOUNDARY_REVIEW.md) | 256 点显示体、3600 万总节点、CPML 色阶、大模型透明度与十套主题检查 |
| 审查记录 | [显示状态与接收选行检查](reviews/SEIS3D_DISPLAY_STATE_RECEIVER_REVIEW.md) | 初始化/重建参数保持、幅值锁定、不同排列、多道记录选行 |
| 审查记录 | [分量尺度与单炮导入检查](reviews/SEIS3D_COMPONENT_SCALE_SHOT_IMPORT_REVIEW.md) | 共同速度参考、压力尺度、单炮 SEG-Y 读取、实际导入截图 |
| 审查记录 | [双语界面与折叠面板检查](reviews/SEIS3D_BILINGUAL_COMPACT_UI_REVIEW.md) | 语言切换、停靠与参数保持、分组收起、中英文实际截图 |
| 审查记录 | [架构和三维升级审查](reviews/DEVELOPMENT_ARCHITECTURE_REVIEW.md) | 三维实现前的架构分析与命名建议 |
| 历史资料 | [原二维 README](history/Seis2D_Original_README.md) | 保留旧版本介绍、界面和性能描述 |

## 目录约定

`user` 放操作文档，`showcase` 放项目展示文章，`publication` 放论文准备与实验计划，`build` 放构建说明，`development` 放实现说明，`reviews` 放审查记录，`history` 放旧版资料。截图按模式集中在 `assets/seis2d`、`assets/seis3d`，文档工具位于 `tools`。构建与产物约定见 [构建说明](build/BUILDING.md)。

本次说明书统一使用 **Paper Sand** 界面主题和 **Paper scientific** 显示预设。截图来自实际程序，局部图仅裁切，不重新绘制波场或修改物理数据。首页和工作区的原始截图均保留在截图目录中。

## 更新说明书

二维指南采用 Markdown，配套图片由 `--capture-seis2d-paper` 生成，再运行 `python docs/tools/prepare_seis2d_screenshots.py` 复制至仓库。原始像素不做后期着色，详见二维指南第 10 节。现有 Word 导出工具针对三维指南，尚未改为二维 Word 导出。

更新 `user/Seis3D_User_Guide.md` 和对应截图后，使用具备 `python-docx`、Pillow 的 Python 运行 `tools/export_user_guide.py`。导出文件写入外层 `out/docs`，排版预览和中间文件写入 `out/docs-work`。

截图可从发布程序的 `--capture-seis3d-paper` 入口重新生成。该入口输出首页、四视图和显示参数页面的 PNG，不保存用户布局或主题偏好。随后从 Git 仓库根目录执行 `python docs/tools/prepare_paper_screenshots.py` 更新截图副本与局部图，再执行 `python docs/tools/export_user_guide.py` 导出 Word。工具根据自身位置定位仓库和外层 `out`。具体操作见 [构建说明](build/BUILDING.md)。

`--capture-seis3d-paper` 临时使用英文；`--capture-seis3d-chinese` 使用中文，并输出 `seis3d-chinese-*.png`。两种截图入口均不保存语言偏好。新版双语与折叠面板截图另以 `home-bilingual-*`、`workspace-compact-zh.png`、`display-compact-zh.png` 命名，保留旧截图供历史对照。本轮更新 Markdown 与截图，Word 需按上述步骤重新导出。

历史文档保留当时的结论和测试条件；三维当前实现以使用指南和实现说明为准。
