#! /usr/bin/env python
import argparse
import sys
from viewer import *
from image_loader import *

def main():
    parser = argparse.ArgumentParser(description="Multi-Volume Viewer")
    parser.add_argument(
        "volumes", nargs="+", help="Volume files and dimensions as: file nx ny nz"
    )
    args = parser.parse_args()

    if len(args.volumes) % 4 != 0:
        raise ValueError("Expected groups of 4 arguments: file nx ny nz")

    volumes = []
    for i in range(0, len(args.volumes), 4):
        try:
            path, nx, ny, nz = args.volumes[i : i + 4]
            vol = load_raw_volume(path, int(nx), int(ny), int(nz))
            volumes.append((vol, int(nx), int(ny), int(nz)))
        except Exception as e:
            raise ValueError(f"Invalid volume specification at position {i}: {e}")

    manager = VolumeViewerManager(volumes)
    sys.exit(manager.app.exec_())


if __name__ == "__main__":
    main()
