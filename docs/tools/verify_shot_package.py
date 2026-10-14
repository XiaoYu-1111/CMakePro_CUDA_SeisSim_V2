"""Verify a SeisWave Lab 2D package with independent Python SHA-256 and format checks."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def verify_package(directory: Path) -> dict:
    """Read-only validation; refuse manifest paths outside the selected package directory."""
    directory = directory.resolve(strict=True)
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"),
                          parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"Invalid JSON number: {value}")))
    if manifest["schema"] != "SeisWaveShot2D-1":
        raise ValueError("Unsupported package schema")
    names = set()
    for item in manifest["files"]:
        relative = Path(item["path"])
        if relative.is_absolute() or ".." in relative.parts or item["path"] in names:
            raise ValueError("Unsafe or duplicated manifest file path")
        names.add(item["path"])
        path = (directory / relative).resolve(strict=True)
        path.relative_to(directory)
        if path.stat().st_size != item["bytes"]:
            raise ValueError(f"File size differs: {relative}")
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if digest != item["sha256"]:
            raise ValueError(f"SHA-256 differs: {relative}")
    required = {"record_vx.sgy", "record_vz.sgy", "geometry.csv"}
    if not required <= names:
        raise ValueError("Required shot files are absent")
    samples, channels = manifest["samples"], manifest["traces"]
    for component in ("vx", "vz"):
        path = directory / f"record_{component}.sgy"
        with path.open("rb") as stream:
            stream.seek(3200)
            binary = stream.read(400)
        interval, count, fmt = struct.unpack_from(">H", binary, 16)[0], struct.unpack_from(">H", binary, 20)[0], struct.unpack_from(">H", binary, 24)[0]
        if (interval, count, fmt) != (manifest["segy_interval_us"], samples, 5):
            raise ValueError("SEG-Y header differs from manifest")
        if path.stat().st_size != 3600 + channels * (240 + samples * 4):
            raise ValueError("SEG-Y file length differs from manifest")
    if manifest["model_included"]:
        if not {"model.s2dm", "source_wavelet.f32le"} <= names:
            raise ValueError("Frozen model or wavelet missing")
        path = directory / "model.s2dm"
        with path.open("rb") as stream:
            magic, nx, nz, spacing = struct.unpack("<8sIIf", stream.read(20))
        grid = manifest["grid"]
        if magic != b"S2DMOD1\0" or (nx, nz, spacing) != (grid["nx"], grid["nz"], grid["spacing_m"]):
            raise ValueError("Frozen model header differs from manifest")
        if path.stat().st_size != 20 + nx * nz * 12:
            raise ValueError("Frozen model payload is truncated")
        if (directory / "source_wavelet.f32le").stat().st_size != manifest["solver"]["nt"] * 4:
            raise ValueError("Frozen source wavelet length differs")
    return {"package": str(directory), "verified_files": len(names), "channels": channels,
            "samples": samples, "raw_units": manifest["units"], "model_included": manifest["model_included"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify_package(args.directory), ensure_ascii=False, indent=2))
