"""Package the maintained blog article and unchanged native screenshots into outer out."""
from pathlib import Path
import shutil
import zipfile


def package_showcase() -> None:
    """Preserve source documents and copy only current, complete image sets."""
    root = Path(__file__).resolve().parents[2]
    docs = root / "docs"
    output = root.parent / "out/blog/Seis3D"
    output.mkdir(parents=True, exist_ok=True)
    def site_paths(value: str) -> str:
        return value.replace("../assets/seis3d/", "/images/seis3d/").replace("../assets/seis2d/", "/images/seis2d/")
    article = (docs / "showcase/CUDA_Seis3D_GitHub_Pages.md").read_text(encoding="utf-8")
    (output / "CUDA_Seis3D.md").write_text(site_paths(article), encoding="utf-8")
    gallery = (docs / "user/Seis3D_Screenshot_Gallery.md").read_text(encoding="utf-8")
    (output / "Seis3D_Screenshot_Gallery.md").write_text(site_paths(gallery), encoding="utf-8")
    folders = ("seis3d/full", "seis2d")
    for folder in folders:
        shutil.copytree(docs / "assets" / folder, output / "images" / folder, dirs_exist_ok=True)
    (output / "README.md").write_text("""# SeisWave Lab 博客发布包

将CUDA_Seis3D.md放入Hexo的source/_posts，将images中的两个目录复制到站点source/images。正文使用/images/seis3d/full和/images/seis2d根路径；若站点部署在子路径，请按站点配置调整前缀。正文保留原发布日期，updated反映本轮更新。

包含三维33张与二维8张原生2560×1494完整PNG，无裁切、拉伸或重新着色。图片摘要见各目录capture-manifest.json，三维完整条件见截图集。三维演示200³、0.5ms、150道×1000点；六种波场色带为同一第480步（0.240s）。二维为独立256×160模型、100道×500点，不能直接与三维比较时间或振幅。

本包不含程序、模型或SEG-Y数据。发布时可设置点击图片查看原图，避免主题降低原图分辨率。\n""", encoding="utf-8")
    package = output.parent / "Seis3D_GitHub_Pages.zip"
    with zipfile.ZipFile(package, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in ("CUDA_Seis3D.md", "README.md", "Seis3D_Screenshot_Gallery.md"):
            archive.write(output / name, name)
        for folder in folders:
            for original in sorted((docs / "assets" / folder).iterdir()):
                if original.suffix.lower() in {".png", ".json"}:
                    archive.write(original, f"images/{folder}/{original.name}")
    print(f"Packaged {package}")


if __name__ == "__main__":
    package_showcase()
