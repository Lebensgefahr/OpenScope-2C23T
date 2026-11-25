#!/usr/bin/env python3
"""Verify the actual committed C initializer in all three configurations."""
import hashlib
import pathlib
import re
import zlib

source = (pathlib.Path(__file__).resolve().parents[1] / "src/fpga_bitstream.c").read_text()
initializer = source.split("const uint8_t fpga_bitstream[] = {", 1)[1].split("};", 1)[0]
tokens = re.findall(r"FPGA_CAPTURE_BYTE\((0x[0-9A-F]{2}), (0x[0-9A-F]{2}), (0x[0-9A-F]{2})\)|(0x[0-9A-F]{2})", initializer)
assert sum(bool(t[0]) for t in tokens) == 51

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
    ("rising", "0a156b73d5b82e230f3cfb5fa1f85af1212a9caa99220873876b00cd5e508ba7", 0xaf04b5c1, 89),
    ("anyedge", "adf5e662073a9ce739e7ead068cba999b39a258f19891b3ec1bf56130d7fd83e", 0x5eb3b33a, 88),
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
    assert sum(a != v for a, v in zip(stock, b)) == (51 if mode else 0)
    print(f"PASS {name}: {len(b)} bytes, CRC {crc:08x}, {bits} changed configuration bits")

# Extract the actual programmed LUTs, not copies of their equations.
from itertools import product
latch_bits = [57225,57228,57226,57227,57866,57867,57865,57868,58505,58508,58506,58507,59146,59147,59145,59148]
mux_bits = [72585,72588,72586,72587,73226,73227,73225,73228,73865,73868,73866,73867,74506,74507,74505,74508]
edge_bits = [67465,67468,67466,67467,68106,68107,68105,68108,68745,68748,68746,68747,69386,69387,69385,69388]
def mask(data, offsets):
    return sum((1 ^ ((data[p//8] >> (7-p%8)) & 1)) << i for i,p in enumerate(offsets))
def lut(m,a,b,c,d):
    return (m >> (a+2*b+4*c+8*d)) & 1
for mode in (1, 2):
    b = payload(mode)
    lm, mm, em = (mask(b,p) for p in (latch_bits,mux_bits,edge_bits))
    assert (lm,mm,em) == (0xaaf0,0xcaca,0x00f2 if mode==1 else 0x00f6)
    for a,x,c,d in product((0,1),repeat=4):
        assert lut(lm,a,x,c,d)==(a if d else c)
        assert lut(mm,a,x,c,d)==(x if c else a)
        assert lut(em,a,x,c,d)==int(not d and (c or ((a and not x) if mode==1 else (a^x))))
    # Repeated arm; transitions on the unselected channel must be ignored.
    for channel in (0,1,0):
        selected=1-channel
        previous=triggered=0
        def tick(pc1,arm,ch1,ch2):
            global selected,previous,triggered
            sample=lut(mm,ch1,ch2,selected,0)
            selected=lut(lm,pc1,0,selected,arm)
            triggered=lut(em,sample,previous,triggered,arm)
            previous=sample
        for _ in range(8):
            tick(channel,1,0,0)
        assert selected==channel and not triggered
        for _ in range(8):
            tick(1-channel,0,channel,1-channel)
            assert not triggered
            tick(1-channel,0,0,0)
            assert not triggered
        tick(1-channel,0,1-channel,channel)
        assert triggered
        assert selected==channel
        tick(1-channel,0,0,0)
        assert triggered
    print(f"PASS mode {mode}: selector, mux and edge LUT truth tables; CH1/CH2 isolation")
