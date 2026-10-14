# SeisWave Lab 文档导航

文档整理日期为 2026 年 10 月 15 日。本目录位于 Git 仓库内部，文档、截图和导出工具可与源码一起提交。按使用模式阅读二维或三维图文说明，再按需要查看构建、算法实现和历史审查。

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

本次三维说明书采用Paper Sand浅色主题，原生2560×1494完整窗口，无裁切、拉伸或外部重着色。33张图片覆盖完整流程和六种波场色带，见[完整窗口截图集](user/Seis3D_Screenshot_Gallery.md)。图片与元数据位于assets/seis3d/full。

## 更新说明书

二维指南采用 Markdown，配套图片由 `--capture-seis2d-paper` 生成，再运行 `python docs/tools/prepare_seis2d_screenshots.py` 复制至仓库。原始像素不做后期着色，详见二维指南第 10 节。现有 Word 导出工具针对三维指南，尚未改为二维 Word 导出。

更新 `user/Seis3D_User_Guide.md` 和对应截图后，使用具备 `python-docx`、Pillow 的 Python 运行 `tools/export_user_guide.py`。导出文件写入外层 `out/docs`，排版预览和中间文件写入 `out/docs-work`。

当前三维截图入口为 `--capture-seis3d-workflow`，临时使用中文Paper Sand与独立演示会话，不保存用户偏好。实际运行200³模型、50×3接收网格，输出33张完整窗口PNG、采集清单和真实模型／SEG-Y数据，写入外层 `out/screenshots/seis3d-full`。

运行 `python docs/tools/prepare_paper_screenshots.py` 原样复制图片并生成截图集；脚本检查分辨率、暂停时刻和文件摘要，不做图像变换。再运行 `python docs/tools/export_user_guide.py` 导出Word。Word正文为A4，截图为独立A3横向页，嵌入原始PNG并保持比例。导出后须用LibreOffice及 `render_docx.py` 渲染，逐页检查分页、文字和图注。排版预览位于外层out/docs-work。

较早的 `--capture-seis3d-paper`、`--capture-seis3d-chinese` 及旧局部截图作为历史资料保留，当前三维指南和博客均使用full目录中的原生截图。

本轮同时更新二维8张2560×1494完整截图及[二维指南](user/Seis2D_User_Guide.md)，包含波场、原始与AGC记录、单道、分量、停靠对照与导出。截图入口为 `--capture-seis2d-workflow`，独立于历史回归入口。

运行 `python docs/tools/package_showcase.py` 可从维护中的博客文章及当前原图重新生成外层 `out/blog/Seis3D/CUDA_Seis3D.md` 和 `out/blog/Seis3D_GitHub_Pages.zip`，保留站点根路径与全部41张完整图片，不打包历史裁切图。Word正文连续排版，截图按编号集中在A3横向附录中。
