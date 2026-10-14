"""Copy actual application captures and crop manual figures without recoloring data."""

from pathlib import Path
import shutil
from PIL import Image


def prepare_screenshots() -> None:
    """Keep full captures and reproducible close-ups in the documentation asset tree."""
    root = Path(__file__).resolve().parents[2]
    source = root.parent / "out/bin/RelWithDebInfo"
    output = root / "docs/assets/seis3d"
    output.mkdir(parents=True, exist_ok=True)
    full_captures = {
        "home-paper.png": "seis3d-paper-home.png",
        "workspace-overview.png": "seis3d-paper-guide.png",
        "display-overview.png": "seis3d-paper-display.png",
        "receiver-row-paper.png": "seis3d-paper-receiver-row.png",
    }
    for name, original in full_captures.items():
        shutil.copy2(source / original, output / name)
    crops = {
        "simulation-controls.png": ("workspace-overview.png", (10, 76, 559, 1054)),
        "inline-slice.png": ("workspace-overview.png", (874, 177, 1265, 543)),
        "spatial-volume.png": ("workspace-overview.png", (1850, 636, 2253, 1042)),
        "toolbar.png": ("workspace-overview.png", (9, 9, 1240, 69)),
        "receiver-traces.png": ("display-overview.png", (574, 1057, 2546, 1397)),
        "display-controls.png": ("display-overview.png", (10, 108, 557, 1125)),
    }
    for name, (original, bounds) in crops.items():
        with Image.open(output / original) as image:
            if image.size != (2560, 1494):
                raise ValueError(f"Unexpected capture dimensions for {original}: {image.size}")
            image.crop(bounds).save(output / name)
    print(f"Prepared {len(full_captures)} full captures and {len(crops)} cropped figures")


if __name__ == "__main__":
    prepare_screenshots()
