#!/usr/bin/env python3
"""Create a reproducible compressed browser payload without extra packages."""
import gzip
import io
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_bytes()
buffer = io.BytesIO()
with gzip.GzipFile(filename="", mode="wb", fileobj=buffer, mtime=0, compresslevel=9) as stream:
    stream.write(source)
payload = buffer.getvalue()
if gzip.decompress(payload) != source:
    raise RuntimeError("Browser payload verification failed")
destination = pathlib.Path(sys.argv[2])
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_bytes(payload)
print(f"Browser payload: {len(source)} -> {len(payload)} bytes")
