#!/usr/bin/env python3
"""Inspect and safely extract the simple ASzf archive used by AlphaSkins."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path, PurePath


def read_u32(data: bytes, offset: int) -> tuple[int, int]:
    if offset + 4 > len(data):
        raise ValueError(f"truncated 32-bit value at 0x{offset:X}")
    return struct.unpack_from("<I", data, offset)[0], offset + 4


def safe_name(raw_name: bytes) -> str:
    try:
        name = raw_name.decode("utf-8")
    except UnicodeDecodeError:
        name = raw_name.decode("cp1252")

    path = PurePath(name)
    if (
        not name
        or path.is_absolute()
        or len(path.parts) != 1
        or name in {".", ".."}
        or "/" in name
        or "\\" in name
        or ":" in name
    ):
        raise ValueError(f"unsafe archive member name: {name!r}")
    return name


def identify(data: bytes, name: str) -> str:
    if data.startswith(b"BM"):
        return "Windows bitmap"
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "PNG image"
    if name.lower().endswith(".dat"):
        try:
            data.decode("cp1252")
            return "text/configuration data"
        except UnicodeDecodeError:
            pass
    return "binary data"


def extract(source: Path, output: Path) -> dict[str, object]:
    blob = source.read_bytes()
    if blob[:4] != b"ASzf":
        raise ValueError("not an ASzf archive")

    file_count, offset = read_u32(blob, 4)
    if file_count > 10_000:
        raise ValueError(f"unreasonable member count: {file_count}")
    if output.exists():
        raise FileExistsError(f"output path already exists: {output}")
    output.mkdir(parents=True)

    members: list[dict[str, object]] = []
    for index in range(file_count):
        record_offset = offset
        name_length, offset = read_u32(blob, offset)
        if name_length == 0 or name_length > 32_768 or offset + name_length > len(blob):
            raise ValueError(f"invalid name length for member {index + 1}")
        name = safe_name(blob[offset : offset + name_length])
        offset += name_length

        expected_size, offset = read_u32(blob, offset)
        if expected_size > 512 * 1024 * 1024:
            raise ValueError(f"unreasonable expanded size for {name}: {expected_size}")
        stream_offset = offset
        if stream_offset + 2 > len(blob):
            raise ValueError(f"missing zlib stream for {name}")

        decoder = zlib.decompressobj()
        payload = decoder.decompress(blob[stream_offset:], expected_size + 1)
        payload += decoder.flush()
        if not decoder.eof:
            raise ValueError(f"incomplete or over-sized zlib stream for {name}")
        if len(payload) != expected_size:
            raise ValueError(
                f"size mismatch for {name}: expected {expected_size}, got {len(payload)}"
            )

        consumed = len(blob) - stream_offset - len(decoder.unused_data)
        if consumed <= 0:
            raise ValueError(f"empty zlib stream for {name}")
        offset = stream_offset + consumed

        destination = output / name
        if destination.exists():
            raise FileExistsError(f"refusing to overwrite: {destination}")
        destination.write_bytes(payload)

        cmf, flg = blob[stream_offset : stream_offset + 2]
        members.append(
            {
                "index": index + 1,
                "name": name,
                "record_offset": record_offset,
                "stream_offset": stream_offset,
                "zlib_header": f"{cmf:02X} {flg:02X}",
                "compressed_size": consumed,
                "expanded_size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest().upper(),
                "type": identify(payload, name),
            }
        )

    if offset != len(blob):
        raise ValueError(f"{len(blob) - offset} unexplained trailing bytes at 0x{offset:X}")

    report: dict[str, object] = {
        "source": str(source.resolve()),
        "source_size": len(blob),
        "source_sha256": hashlib.sha256(blob).hexdigest().upper(),
        "magic": blob[:4].decode("ascii"),
        "file_count": file_count,
        "parsed_bytes": offset,
        "trailing_bytes": 0,
        "members": members,
    }
    (output / "manifest.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        report = extract(args.source, args.output)
    except (OSError, ValueError, zlib.error) as exc:
        print(f"inspect_aszf: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
