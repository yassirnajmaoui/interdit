#!/usr/bin/env python
import numpy as np


def load_raw_volume(filename, nx, ny, nz):
    """Load volume from raw binary file"""
    try:
        data = np.fromfile(filename, dtype=np.float32)
        expected_size = nx * ny * nz
        if len(data) != expected_size:
            raise ValueError(
                f"File size mismatch. Expected {expected_size} elements "
                f"({nx}x{ny}x{nz}), got {len(data)}"
            )
        return data.reshape((nz, ny, nx))  # Note ZYX ordering for numpy
    except Exception as e:
        raise RuntimeError(f"Error loading volume: {str(e)}")

