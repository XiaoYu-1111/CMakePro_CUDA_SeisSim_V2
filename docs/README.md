# 项目文档导航

文档整理日期为 2026 年 10 月 12 日。本目录位于 Git 仓库内部，文档、截图和导出工具可与源码一起提交。推荐先阅读三维图文使用说明，再按需要查看构建、算法实现和历史审查。

| 分类 | 文档 | 内容 |
| --- | --- | --- |
| 使用说明 | [Seis3D 图文指南](user/Seis3D_User_Guide.md) | 首页、计算参数、切片、浅色样式、100 道采集、模型和 SEG-Y 导出 |
| 可编辑说明书 | Word 导出版本 | 从图文指南生成，保存到外层 `out/docs/Seis3D_User_Guide.docx` |
| 构建 | [BUILDING](build/BUILDING.md) | VS、命令行、外层 out、资源部署和增量编译 |
| 实现 | [SEIS3D_IMPLEMENTATION](development/SEIS3D_IMPLEMENTATION.md) | 三维隔离、差分、CPML、自由表面、显存和文件格式 |
| 审查记录 | [数值稳定性审查](reviews/NUMERICAL_STABILITY_REVIEW.md) | 二维求解器修复与当时的回归结果 |
| 审查记录 | [架构和三维升级审查](reviews/DEVELOPMENT_ARCHITECTURE_REVIEW.md) | 三维实现前的架构分析与命名建议 |
| 历史资料 | [原二维 README](history/Seis2D_Original_README.md) | 保留旧版本介绍、界面和性能描述 |

## 目录约定

`user` 放操作文档，`build` 放构建说明，`development` 放实现说明，`reviews` 放审查记录，`history` 放旧版资料。截图集中在 `assets/seis3d`，Word 导出工具位于 `tools`。构建与产物约定见 [构建说明](build/BUILDING.md)。

本次说明书统一使用 **Paper Sand** 界面主题和 **Paper scientific** 显示预设。截图来自实际程序，局部图仅裁切，不重新绘制波场或修改物理数据。首页和工作区的原始截图均保留在截图目录中。

## 更新说明书

更新 `user/Seis3D_User_Guide.md` 和对应截图后，使用具备 `python-docx`、Pillow 的 Python 运行 `tools/export_user_guide.py`。导出文件写入外层 `out/docs`，排版预览和中间文件写入 `out/docs-work`。

截图可从发布程序的 `--capture-seis3d-paper` 入口重新生成。该入口输出首页、四视图和显示参数页面的 PNG，不保存用户布局或主题偏好。随后从 Git 仓库根目录执行 `python docs/tools/prepare_paper_screenshots.py` 更新截图副本与局部图，再执行 `python docs/tools/export_user_guide.py` 导出 Word。工具根据自身位置定位仓库和外层 `out`。具体操作见 [构建说明](build/BUILDING.md)。

历史文档保留当时的结论和测试条件；三维当前实现以使用指南和实现说明为准。
