#!/usr/bin/env python3
import struct
import sys
import zlib


def load_png(path):
    raw = open(path, 'rb').read()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    width = height = color_type = None
    compressed = bytearray()
    while pos < len(raw):
        length = struct.unpack('>I', raw[pos:pos + 4])[0]
        kind = raw[pos + 4:pos + 8]
        data = raw[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            width, height, depth, color_type, _, _, interlace = struct.unpack('>IIBBBBB', data)
            assert depth == 8 and color_type == 6 and interlace == 0
        elif kind == b'IDAT':
            compressed.extend(data)
        elif kind == b'IEND':
            break
    decoded = zlib.decompress(compressed)
    bpp = 4
    stride = width * bpp
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        filt = decoded[p]
        p += 1
        row = bytearray(decoded[p:p + stride])
        p += stride
        for i in range(stride):
            left = row[i - bpp] if i >= bpp else 0
            up = prev[i]
            up_left = prev[i - bpp] if i >= bpp else 0
            if filt == 1:
                row[i] = (row[i] + left) & 255
            elif filt == 2:
                row[i] = (row[i] + up) & 255
            elif filt == 3:
                row[i] = (row[i] + ((left + up) // 2)) & 255
            elif filt == 4:
                q = left + up - up_left
                pa, pb, pc = abs(q - left), abs(q - up), abs(q - up_left)
                pr = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
                row[i] = (row[i] + pr) & 255
            elif filt != 0:
                raise AssertionError(f'unsupported filter {filt}')
        rows.append(row)
        prev = row
    return width, height, rows


def muted_pixels(rows, box):
    x1, y1, x2, y2 = box
    count = 0
    for y in range(y1, y2):
        row = rows[y]
        for x in range(x1, x2):
            r, g, b = row[x * 4:x * 4 + 3]
            if abs(r - 0x6B) <= 18 and abs(g - 0x72) <= 18 and abs(b - 0x80) <= 18:
                count += 1
    return count


path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/power_axis_labels.png'
w, h, rows = load_png(path)
assert (w, h) == (320, 240), (w, h)
# The simulator cards are 148 px wide; each chart's future right-side label
# column is inside these four regions.
regions = [(110, 65, 153, 110), (266, 65, 309, 110),
           (110, 185, 153, 230), (266, 185, 309, 230)]
counts = [muted_pixels(rows, box) for box in regions]
print('muted-pixel counts:', counts)
assert all(c >= 8 for c in counts), 'expected two visible axis numbers in every chart'

# Layout regression: the chart must own the row height and the numeric axis
# should be only as wide as the longest label, not a visibly padded column.
source = open("src/ui.c", "r", encoding="utf-8").read()
assert "lv_obj_set_width(axis, 33);" in source, "axis column is still too wide"
assert "lv_obj_set_height(area, LV_PCT(100));" in source, "chart does not fill the chart row"
assert "lv_obj_set_style_text_align(max_label, LV_TEXT_ALIGN_RIGHT, 0);" in source
assert "lv_obj_set_style_text_align(min_label, LV_TEXT_ALIGN_RIGHT, 0);" in source
