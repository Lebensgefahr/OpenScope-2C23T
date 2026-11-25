#!/usr/bin/env python3
"""Verify the actual committed C initializer in all three configurations."""
import hashlib
import pathlib
import re
import zlib

source = (pathlib.Path(__file__).resolve().parents[1] / "src/fpga_bitstream.c").read_text()
initializer = source.split("const uint8_t fpga_bitstream[] = {", 1)[1].split("};", 1)[0]
tokens = re.findall(r"FPGA_CAPTURE_BYTE\((0x[0-9A-F]{2}), (0x[0-9A-F]{2}), (0x[0-9A-F]{2})\)|(0x[0-9A-F]{2})", initializer)
assert sum(bool(t[0]) for t in tokens) == 33

def payload(mode):
    return bytes(int(t[mode] if t[0] else t[3], 16) for t in tokens)

def agm_crc(data):
    c = 0xffffffff
    for b in data:
        c ^= b << 24
        for _ in range(8):
            c = ((c << 1) ^ (0x04c11db7 if c & 0x80000000 else 0)) & 0xffffffff
    return c ^ 0xffffffff

expected = [
    ("stock", "7c9ad508c29df5f4df77fd7759fc7aa58eaebcc927843b294a5ac4dacce8edd5", 0x53ae245f, 0),
    ("rising", "50ce417c1e68592a8dd9ededdbec9de3a7724e4d8ce2f53e8b28523cec3df723", 0x0e379cfd, 53),
    ("anyedge", "4c3a70916aceb67e6a75be36bd995b9a39747e2de2051d3a4afc4de171ac2379", 0xff809a06, 52),
]
stock = payload(0)
assert zlib.crc32(stock) == 0xb76f3d95
for mode, (name, sha, crc, bits) in enumerate(expected):
    b = payload(mode)
    assert len(b) == 46808
    assert hashlib.sha256(b).hexdigest() == sha
    assert agm_crc(b[:-4]) == int.from_bytes(b[-4:], "big") == crc
    assert sum((a ^ v).bit_count() for a, v in zip(stock[:-4], b[:-4])) == bits
    assert b[:76] == stock[:76] and b[-12:-4] == stock[-12:-4]
    assert sum(a != v for a, v in zip(stock, b)) == (33 if mode else 0)
    print(f"PASS {name}: {len(b)} bytes, CRC {crc:08x}, {bits} changed configuration bits")
