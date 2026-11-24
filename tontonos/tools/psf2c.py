#!/usr/bin/env python3
"""Convertit une police console PSF (v1 ou v2) en tableau C 8x16 indexé en Latin-1."""
import gzip, struct, sys

def load(path):
    data = gzip.open(path).read() if path.endswith('.gz') else open(path, 'rb').read()
    if data[:2] == b'\x36\x04':  # PSF1
        mode, h = data[2], data[3]
        n = 512 if mode & 1 else 256
        glyphs = [data[4 + i*h: 4 + (i+1)*h] for i in range(n)]
        table = data[4 + n*h:] if mode & 2 else b''
        umap = {}
        if table:
            i = 0
            for g in range(n):
                while True:
                    cp = struct.unpack_from('<H', table, i)[0]; i += 2
                    if cp == 0xFFFF: break
                    if cp == 0xFFFE: continue
                    umap.setdefault(cp, g)
        return glyphs, h, umap
    magic, ver, hs, flags, n, gsize, h, w = struct.unpack_from('<8I', data)
    assert magic == 0x864ab572 and w == 8
    glyphs = [data[hs + i*gsize: hs + (i+1)*gsize] for i in range(n)]
    umap = {}
    if flags & 1:
        t = data[hs + n*gsize:]
        g = 0; cur = bytearray()
        for b in t:
            if b == 0xFF:
                for s in cur.decode('utf-8', 'ignore').split('￾')[0]:
                    umap.setdefault(ord(s), g)
                g += 1; cur = bytearray()
            else:
                cur.append(b)
    return glyphs, h, umap

glyphs, h, umap = load(sys.argv[1])
assert h == 16
out = ['/* Généré par tools/psf2c.py depuis ' + sys.argv[1].split('/')[-1] + ' — ne pas modifier. */',
       '#ifndef FONT_DATA_H', '#define FONT_DATA_H', 'static const unsigned char font8x16[256][16] = {']
for cp in range(256):
    g = umap.get(cp, umap.get(ord('?'), 0)) if umap else cp
    if cp < 32 or 127 <= cp < 160: g = umap.get(ord(' '), 0) if umap else 0
    out.append('  {' + ','.join('0x%02x' % b for b in glyphs[g]) + '},')
out += ['};', '#endif']
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
