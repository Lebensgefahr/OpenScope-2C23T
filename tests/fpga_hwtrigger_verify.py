#!/usr/bin/env python3
"""Verify the committed bitstream and model its decoded trigger LUTs (not STA)."""
import hashlib
import itertools
import pathlib
import re
import zlib

source = (pathlib.Path(__file__).resolve().parents[1] / "src/fpga_bitstream.c").read_text()
initializer = source.split("const uint8_t fpga_bitstream[] = {", 1)[1].split("};", 1)[0]
tokens = re.findall(r"FPGA_CAPTURE_BYTE\((0x[0-9A-F]{2}), (0x[0-9A-F]{2}), (0x[0-9A-F]{2})\)|(0x[0-9A-F]{2})", initializer)
assert sum(bool(t[0]) for t in tokens) == 442

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
    ("selectable edge", "8c9bb3a4e2e9ff551b6cd45bd25cec27c9013fe5a730cd35f56fb408cf442ada", 0xd304b77a, 750),
    ("anyedge", "cc4b5a39ad91ae4b44b5bc92516f6d2d240560ed38f191cdc66af64081e1c1d8", 0x22b3b181, 749),
]
stock = payload(0)
assert zlib.crc32(stock) == 0xb76f3d95
for mode, (name, sha, crc, bits) in enumerate(expected):
    b = payload(mode)
    assert len(b) == 46808 and hashlib.sha256(b).hexdigest() == sha
    assert agm_crc(b[:-4]) == int.from_bytes(b[-4:], "big") == crc
    assert sum((a ^ v).bit_count() for a, v in zip(stock[:-4], b[:-4])) == bits
    assert b[:76] == stock[:76] and b[-12:-4] == stock[-12:-4]
    assert sum(a != v for a, v in zip(stock, b)) == (442 if mode else 0)
    print(f"PASS {name}: CRC {crc:08x}, {bits} changed configuration bits")

W = [34,34,34,108,34,34,34,34,34,34,108,34,34,16]
H = [20] + [68]*8 + [20]
LUT = [30,27,29,28,63,62,64,61,98,95,97,96,131,130,132,129]

def mask(data,x,y,s):
    base = s*136 + (136 if s>=8 else 0)
    out = 0
    for i,k in enumerate(LUT):
        row,col = divmod(base+k,34)
        bit = 76*8 + (sum(H[y+1:])+row)*640 + sum(W[x+1:]) + (33-col)
        out |= (1-(data[bit//8]>>(7-bit%8)&1))<<i
    return out

def lut(m,a=0,b=0,c=0,d=0):
    return m>>(a+2*b+4*c+8*d)&1

locs = {'sync1':(6,5,1),'sync2':(6,5,0),'sync3':(6,5,2),'strobe':(6,5,9),
        'sel':(6,7,0),'pol':(6,7,3),'sample':(4,7,3),'filter':(4,7,14),
        'prev':(6,7,1),'edge':(6,7,4)}
shift = [(6,5,s) for s in (13,12,14,3,7,11,10,6,8,5)]
compare = [[(6,7,12),(6,7,2),(6,7,8),(5,7,14),(5,7,11),(5,7,15),(5,7,10),(6,7,15)],
           [(2,5,4),(2,5,2),(2,5,14),(2,5,3),(1,5,14),(1,5,2),(1,5,3),(2,5,10)]]

def verify(mode):
    b = payload(mode)
    m = {k:mask(b,*v) for k,v in locs.items()}
    sm = [mask(b,*v) for v in shift]
    cm = [[mask(b,*v) for v in row] for row in compare]
    assert m == {'sync1':0xaaaa,'sync2':0xaaaa,'sync3':0xaaaa,'strobe':0x2222,
                 'sel':0xaaf0,'pol':0x35ca,'sample':0xaaaa,'filter':0xe8e8,
                 'prev':0xaaaa,'edge':0x00f2 if mode==1 else 0x00f6}
    assert sm == [0xaaf0]*10 and cm == [[0xbbbb]+[0xb2b2]*7]*2

    def cmp(ch,val,level):
        c = 0
        for i,tab in enumerate(cm[ch]):
            c = lut(tab,val>>i&1,level>>i&1,c)
        return c

    for ch,level,val in itertools.product(range(2),range(256),range(256)):
        assert cmp(ch,val,level) == int(val>=level)
    for x,y,ch,fall in itertools.product(range(2),repeat=4):
        assert lut(m['pol'],x,y,ch,fall) == ((y if ch else x)^fall)

    for channel,level,fall in itertools.product(range(2),range(256),range(2)):
        word = fall<<9|level<<1|channel
        q = [1]*10
        s1=s2=s3=0
        sel=1-channel
        def tick(arm,pc1):
            nonlocal q,s1,s2,s3,sel
            enable=lut(m['strobe'],s2,s3)
            q=[lut(tab,a,0,c,enable) for tab,a,c in zip(sm,[pc1]+q[:-1],q)]
            s1,s2,s3=lut(m['sync1'],arm),lut(m['sync2'],s1),lut(m['sync3'],s2)
            sel=lut(m['sel'],pc1,0,sel,arm)
        for i in range(9,-1,-1):
            v=word>>i&1
            for _ in range(8): tick(0,v)
            for _ in range(8): tick(1,v)
        for _ in range(8): tick(0,1-channel)
        assert sum(v<<i for i,v in enumerate(q))==word and sel==channel

    for a,bv,c in itertools.product(range(2),repeat=3):
        assert lut(m['filter'],a,bv,c)==(a if a==bv else c)
    for ch,fall in itertools.product(range(2),repeat=2):
        lo,hi=(63,192) if not fall else (192,63)
        prev=filt=sample=trig=0
        def tick_signal(arm,ch1,ch2):
            nonlocal prev,filt,sample,trig
            x=lut(m['pol'],cmp(0,ch1,128),cmp(1,ch2,128),ch,fall)
            trig,prev,filt,sample=(lut(m['edge'],filt,prev,trig,arm),
                                  lut(m['prev'],filt),lut(m['filter'],x,sample,filt),
                                  lut(m['sample'],x))
        for _ in range(8): tick_signal(1,lo,lo)
        for _ in range(4): tick_signal(0,lo,lo)
        assert not trig
        for _ in range(8): tick_signal(0,lo if ch==0 else hi,hi if ch==0 else lo)
        assert not trig
        for _ in range(8): tick_signal(0,hi if ch==0 else lo,lo if ch==0 else hi)
        assert trig
    print(f"PASS mode {mode}: 131072 comparisons, 1024 serial words, 4 edge/isolation scenarios")

verify(1)
verify(2)
