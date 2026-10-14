"""Copy genuine 2D application captures into the tracked documentation folder."""
from pathlib import Path
import shutil


def prepare_screenshots() -> None:
    """Preserve original PNG pixels; do not synthesize or recolor scientific results."""
    root = Path(__file__).resolve().parents[2]
    source = root.parent / "out/bin/RelWithDebInfo"
    output = root / "docs/assets/seis2d"
    captures = {
        "wavefield-paper.png": "seis2d-wavefield-paper.png",
        "record-raw.png": "seis2d-record-raw.png",
        "record-agc.png": "seis2d-record-agc.png",
        "single-vz.png": "seis2d-single-vz.png",
        "section-vx.png": "seis2d-section-vx.png",
        "docking-analysis.png": "seis2d-docking-analysis.png",
        "docking-record-focus.png": "seis2d-docking-record-focus.png",
        "export-package.png": "seis2d-export-package.png",
    }
    # Check all inputs before replacing documentation images.
    for original in captures.values():
        path = source / original
        if not path.is_file():
            raise FileNotFoundError(f"Run SeisWaveLab.exe --capture-seis2d-paper first: {path}")
    output.mkdir(parents=True, exist_ok=True)
    for name, original in captures.items():
        shutil.copy2(source / original, output / name)
    print(f"Copied {len(captures)} original 2D captures to {output}")


if __name__ == "__main__":
    prepare_screenshots()
