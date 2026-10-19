#!/usr/bin/env python3
"""
py2g3a - turn a Python script into a Casio fx-CG50 / fx-CG20 / fx-CG10 add-in (.g3a).

Every script you convert becomes its own app in the calculator's MAIN MENU,
with its own name and icon.

How it works
------------
A .g3a has to contain compiled SuperH machine code, so a .py can't be
"compiled" into one directly. Instead this tool takes a ready-made add-in,
runner.g3a (a small MicroPython interpreter), and stitches your script onto
the end of it. It then rewrites the header (name, icons, file size,
checksums) so the calculator treats the result as a brand-new add-in.

No compilers, no extra packages: just Python 3.8+ on Windows, macOS or Linux.

Usage
-----
    python py2g3a.py game.py                      -> game.g3a
    python py2g3a.py game.py -n "Snake" -i snake.png
    python py2g3a.py                              (no arguments: opens a small window)

Copy the .g3a to the root of the calculator's storage over USB
(choose "USB Flash" on the calculator) and it appears in MAIN MENU.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys
import time
import zlib

__version__ = "1.0.0"

# --------------------------------------------------------------------------
# G3A format constants (layout from mkg3a's g3a.h and the WikiPrizm page)
# --------------------------------------------------------------------------

HEADER_SIZE = 0x7000
MAGIC = bytes([0xAA, 0xAC, 0xBD, 0xAF, 0x90, 0x88, 0x9A, 0x8D, 0xD3, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF])

OFF_CPROT = 0x000E          # 7 "copy protection" bytes derived from file size
OFF_CKSUM = 0x0020          # u32 BE: byte sum of file with both checksum fields zeroed
OFF_CODE_SIZE = 0x002E      # u32 BE: code size == offset of checksum copy from 0x7000
OFF_NAME_BASIC = 0x0040     # 0x1C bytes
OFF_FILE_SIZE = 0x005C      # u32 BE
OFF_NAME_INTERNAL = 0x0060  # 0x0B bytes, "@NAME"
OFF_LC_NAMES = 0x006B       # 8 x 0x18 bytes (en es de fr pt zh + 2 spare)
OFF_VERSION = 0x0130        # 0x0C bytes "01.00.0000"
OFF_TIMESTAMP = 0x013C      # 0x0F bytes "YYYY.MMDD.HHMM"
OFF_FILENAME = 0x0EBC       # 0x144 bytes
OFF_ICON_UNSEL = 0x1000
OFF_ICON_SEL = 0x4000

ICON_W, ICON_H = 92, 64
ICON_BYTES = ICON_W * ICON_H * 2

# The runner reserves a "slot" at the very end of its code. Layout:
#   0  16 bytes  magic
#   16 u32 BE    slot format version (1)
#   20 u32 BE    script length in bytes (0 = no script)
#   24 u32 BE    CRC-32 of the script bytes
#   28 u32 BE    reserved (0)
#   32 ...       script bytes, then a NUL, then padding to a multiple of 4
SLOT_MAGIC = b"PY2G3A:SLOT:v1\x00\x00"
SLOT_HEADER = 32

# The OS refuses add-ins much bigger than this; stay comfortably under.
MAX_G3A_SIZE = 2 * 1024 * 1024


class ConvertError(Exception):
    """A problem the user can fix (bad input, missing runner, and so on)."""


# --------------------------------------------------------------------------
# Small helpers
# --------------------------------------------------------------------------

def byte_sum(data) -> int:
    """mkg3a's checksum: plain sum of every byte, truncated to 32 bits."""
    return sum(data) & 0xFFFFFFFF


def put_str(buf: bytearray, off: int, size: int, text: str) -> None:
    """Write a NUL-padded string field (always leaves room for a NUL)."""
    raw = text.encode("ascii", "replace")[: size - 1]
    buf[off:off + size] = raw + b"\x00" * (size - len(raw))


def safe_ascii(text: str) -> str:
    """The calculator's menu font is ASCII; replace anything else."""
    out = []
    for ch in text:
        out.append(ch if 32 <= ord(ch) < 127 else "_")
    return "".join(out).strip() or "Python"


def internal_name(display_name: str) -> str:
    """
    The OS identifies add-ins by an internal name like "@SNAKE" (A-Z only).
    Two add-ins with the same internal name clash, so add a short hash of the
    full name to keep every converted script unique.
    """
    letters = "".join(c for c in display_name.upper() if "A" <= c <= "Z")[:5] or "PY"
    digest = hashlib.sha1(display_name.encode("utf-8")).digest()
    tag = "".join(chr(ord("A") + b % 26) for b in digest[:8 - len(letters)])
    return "@" + letters + tag


# --------------------------------------------------------------------------
# Reading and validating the runner
# --------------------------------------------------------------------------

def check_g3a(data: bytes, what: str = "file") -> None:
    """Raise ConvertError unless `data` is a structurally valid g3a."""
    if len(data) < HEADER_SIZE + 4:
        raise ConvertError(f"{what} is too small to be a .g3a add-in")
    if data[:14] != MAGIC:
        raise ConvertError(f"{what} is not a .g3a add-in (bad magic bytes)")
    file_size = struct.unpack_from(">I", data, OFF_FILE_SIZE)[0]
    code_size = struct.unpack_from(">I", data, OFF_CODE_SIZE)[0]
    if file_size != len(data) or code_size + HEADER_SIZE + 4 != len(data):
        raise ConvertError(f"{what} has inconsistent size fields (truncated or corrupted?)")
    stored = struct.unpack_from(">I", data, OFF_CKSUM)[0]
    tail = struct.unpack_from(">I", data, len(data) - 4)[0]
    computed = (byte_sum(data) - byte_sum(data[OFF_CKSUM:OFF_CKSUM + 4]) - byte_sum(data[-4:])) & 0xFFFFFFFF
    if not (stored == tail == computed):
        raise ConvertError(f"{what} has a bad checksum (corrupted download?)")


def find_slot(code: bytes) -> int:
    """Return the offset (inside the code section) of the runner's script slot."""
    pos = code.rfind(SLOT_MAGIC)
    if pos < 0:
        raise ConvertError(
            "runner.g3a doesn't contain a py2g3a script slot. "
            "Make sure you're using the runner.g3a that came with this tool."
        )
    if pos + SLOT_HEADER > len(code):
        raise ConvertError("runner.g3a's script slot is truncated")
    version = struct.unpack_from(">I", code, pos + 16)[0]
    if version != 1:
        raise ConvertError(f"runner.g3a has slot format v{version}; this tool only knows v1")
    return pos


# --------------------------------------------------------------------------
# Icons
# --------------------------------------------------------------------------

def rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def pixels_to_icon(pixels) -> bytes:
    """pixels: list of ICON_H rows, each ICON_W (r, g, b) tuples -> big-endian RGB565."""
    out = bytearray()
    for row in pixels:
        for r, g, b in row:
            out += struct.pack(">H", rgb565(r, g, b))
    return bytes(out)


# 5x7 bitmap font for the auto-generated icon label. Each glyph is 7 rows of 5 bits.
_FONT = {
    "A": "01110 10001 10001 11111 10001 10001 10001", "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110", "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111", "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111", "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110", "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001", "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001", "N": "10001 11001 10101 10011 10001 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110", "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101", "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110", "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110", "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 10101 01010", "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100", "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110", "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111", "3": "11111 00010 00100 00010 00001 10001 01110",
    "4": "00010 00110 01010 10010 11111 00010 00010", "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110", "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110", "9": "01110 10001 10001 01111 00001 00010 01100",
    " ": "00000 00000 00000 00000 00000 00000 00000", "-": "00000 00000 00000 11111 00000 00000 00000",
    "_": "00000 00000 00000 00000 00000 00000 11111", ".": "00000 00000 00000 00000 00000 01100 01100",
    "!": "00100 00100 00100 00100 00100 00000 00100", "?": "01110 10001 00001 00010 00100 00000 00100",
    "+": "00000 00100 00100 11111 00100 00100 00000", ">": "01000 00100 00010 00001 00010 00100 01000",
    "'": "00100 00100 01000 00000 00000 00000 00000", "&": "01100 10010 10100 01000 10101 10010 01101",
}


def _draw_text(pix, text: str, cx: int, y: int, scale: int, color) -> None:
    text = "".join(c if c in _FONT else "?" for c in text.upper())
    width = len(text) * 6 * scale - scale
    x0 = cx - width // 2
    for i, ch in enumerate(text):
        rows = _FONT[ch].split()
        for ry, bits in enumerate(rows):
            for rx, bit in enumerate(bits):
                if bit == "1":
                    for dy in range(scale):
                        for dx in range(scale):
                            px, py = x0 + i * 6 * scale + rx * scale + dx, y + ry * scale + dy
                            if 0 <= px < ICON_W and 0 <= py < ICON_H:
                                pix[py][px] = color


def _fit_label(name: str):
    """Pick the biggest font scale (and maybe truncate) so the label fits."""
    label = name.upper()
    for scale in (2, 1):
        max_chars = (ICON_W - 4 + scale) // (6 * scale)
        if len(label) <= max_chars:
            return label, scale
    return label[:(ICON_W - 4 + 1) // 6], 1


def make_default_icons(name: str):
    """Generate a tidy icon pair: a coloured tile with '>_' and the app name."""
    # Pick a stable colour per app so different scripts look different.
    palette = [(32, 96, 200), (200, 60, 50), (40, 150, 80), (150, 70, 180), (220, 130, 20), (20, 140, 160)]
    tile = palette[hashlib.sha1(name.encode("utf-8")).digest()[0] % len(palette)]
    label, scale = _fit_label(name)

    def build(selected: bool):
        bg = (255, 255, 255)
        pix = [[bg] * ICON_W for _ in range(ICON_H)]
        # Rounded tile in the upper part.
        x0, y0, x1, y1 = 26, 4, 66, 40
        for y in range(y0, y1):
            for x in range(x0, x1):
                corner = (min(x - x0, x1 - 1 - x), min(y - y0, y1 - 1 - y))
                if corner[0] + corner[1] >= 3 or min(corner) >= 2:
                    pix[y][x] = tile
        _draw_text(pix, ">_", 46, 15, 2, (255, 255, 255))
        text_y = 46 if scale == 2 else 50
        _draw_text(pix, label, ICON_W // 2, text_y, scale, (0, 0, 0))
        if selected:
            # Casio's own selected icons get a coloured frame; do the same.
            frame = (20, 60, 160)
            for t in range(3):
                for x in range(ICON_W):
                    pix[t][x] = pix[ICON_H - 1 - t][x] = frame
                for y in range(ICON_H):
                    pix[y][t] = pix[y][ICON_W - 1 - t] = frame
        return pixels_to_icon(pix)

    return build(False), build(True)


# ---- minimal PNG reader (stdlib only) ------------------------------------

def read_png(path: str):
    """Decode a PNG into (width, height, rows of (r, g, b, a))."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ConvertError(f"{path} isn't a PNG file")
    pos, idat, palette, trns = 8, bytearray(), None, None
    width = height = depth = ctype = interlace = None
    while pos + 8 <= len(data):
        length, kind = struct.unpack_from(">I4s", data, pos)
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"PLTE":
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif kind == b"tRNS":
            trns = chunk
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
    if width is None:
        raise ConvertError(f"{path}: missing PNG header")
    if interlace:
        raise ConvertError(f"{path} is an interlaced PNG; re-save it without interlacing")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(ctype)
    if channels is None or depth not in (1, 2, 4, 8, 16) or (depth < 8 and ctype not in (0, 3)):
        raise ConvertError(f"{path}: unsupported PNG type; re-save it as an ordinary RGB/RGBA PNG")
    raw = zlib.decompress(bytes(idat))
    bits_pp = channels * depth
    stride = (width * bits_pp + 7) // 8
    bpp = max(1, bits_pp // 8)
    prev = bytearray(stride)
    rows, i = [], 0
    for _ in range(height):
        ftype = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if ftype == 1:
                line[x] = (line[x] + a) & 0xFF
            elif ftype == 2:
                line[x] = (line[x] + b) & 0xFF
            elif ftype == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        prev = line
        # Unpack samples.
        if depth < 8:
            samples = []
            for byte in line:
                for shift in range(8 - depth, -1, -depth):
                    samples.append((byte >> shift) & ((1 << depth) - 1))
            samples = samples[:width]
        elif depth == 8:
            samples = list(line)
        else:
            samples = [line[k] for k in range(0, len(line), 2)]  # keep high byte of 16-bit
        row = []
        for x in range(width):
            if ctype == 3:
                idx = samples[x]
                r, g, b = palette[idx] if palette and idx < len(palette) else (0, 0, 0)
                alpha = trns[idx] if trns and idx < len(trns) else 255
            elif ctype == 0:
                v = samples[x] * 255 // ((1 << depth) - 1) if depth < 8 else samples[x]
                r = g = b = v
                alpha = 255
            elif ctype == 4:
                r = g = b = samples[2 * x]
                alpha = samples[2 * x + 1]
            elif ctype == 2:
                r, g, b = samples[3 * x:3 * x + 3]
                alpha = 255
            else:
                r, g, b, alpha = samples[4 * x:4 * x + 4]
            row.append((r, g, b, alpha))
        rows.append(row)
    return width, height, rows


def load_icon(path: str) -> bytes:
    """Load a PNG, scale it to 92x64 (keeping aspect ratio) on white, return RGB565."""
    w, h, rows = read_png(path)
    scale = min(ICON_W / w, ICON_H / h)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    ox, oy = (ICON_W - nw) // 2, (ICON_H - nh) // 2
    pix = [[(255, 255, 255)] * ICON_W for _ in range(ICON_H)]
    for y in range(nh):
        sy = min(h - 1, int(y / scale))
        for x in range(nw):
            sx = min(w - 1, int(x / scale))
            r, g, b, a = rows[sy][sx]
            # Blend onto white so transparent PNGs look right.
            pix[oy + y][ox + x] = tuple((c * a + 255 * (255 - a)) // 255 for c in (r, g, b))
    return pixels_to_icon(pix)


# --------------------------------------------------------------------------
# The conversion itself
# --------------------------------------------------------------------------

def default_runner_path() -> str:
    here = os.path.dirname(os.path.abspath(sys.argv[0] if __name__ == "__main__" else __file__))
    return os.path.join(here, "runner.g3a")


def build_g3a(runner: bytes, script: bytes, name: str, out_filename: str,
              icon_unsel: bytes | None = None, icon_sel: bytes | None = None,
              version: str = "01.00.0000") -> bytes:
    """Return the bytes of a new .g3a running `script`."""
    check_g3a(runner, "runner.g3a")

    # Normalise the script: UTF-8, Unix newlines, no NUL bytes (the runner
    # treats the script as a C string).
    try:
        text = script.decode("utf-8-sig")
    except UnicodeDecodeError:
        raise ConvertError("the script isn't valid UTF-8 text")
    if "\x00" in text:
        raise ConvertError("the script contains a NUL byte")
    payload = text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")
    if not payload.strip():
        raise ConvertError("the script is empty")

    code = bytearray(runner[HEADER_SIZE:-4])
    slot = find_slot(bytes(code))

    # Rebuild the slot: header + script + NUL + padding.
    body = payload + b"\x00"
    body += b"\x00" * (-len(body) % 4)
    slot_header = SLOT_MAGIC + struct.pack(">IIII", 1, len(payload), zlib.crc32(payload) & 0xFFFFFFFF, 0)
    code = code[:slot] + slot_header + body

    total = HEADER_SIZE + len(code) + 4
    if total > MAX_G3A_SIZE:
        raise ConvertError(f"the result would be {total // 1024} KB; the calculator's limit is about 2 MB")

    header = bytearray(runner[:HEADER_SIZE])

    # Sizes and the size-derived "copy protection" bytes (as mkg3a does it).
    struct.pack_into(">I", header, OFF_CODE_SIZE, len(code))
    struct.pack_into(">I", header, OFF_FILE_SIZE, total)
    inv = (~total) & 0xFFFFFFFF
    struct.pack_into(">I", header, OFF_CPROT + 2, inv)
    header[OFF_CPROT + 0] = (header[OFF_CPROT + 5] - 0x41) & 0xFF
    header[OFF_CPROT + 1] = 0xFE
    header[OFF_CPROT + 6] = (header[OFF_CPROT + 5] - 0xB8) & 0xFF

    # Names.
    name = safe_ascii(name)
    put_str(header, OFF_NAME_BASIC, 0x1C, name)
    put_str(header, OFF_NAME_INTERNAL, 0x0B, internal_name(name))
    for i in range(8):
        put_str(header, OFF_LC_NAMES + i * 0x18, 0x18, name)
    put_str(header, OFF_FILENAME, 0x144, os.path.basename(out_filename))
    put_str(header, OFF_VERSION, 0x0C, version)
    put_str(header, OFF_TIMESTAMP, 0x0F, time.strftime("%Y.%m%d.%H%M"))

    # Icons.
    if icon_unsel is None or icon_sel is None:
        auto_unsel, auto_sel = make_default_icons(name)
        icon_unsel = icon_unsel or auto_unsel
        icon_sel = icon_sel or auto_sel
    assert len(icon_unsel) == ICON_BYTES and len(icon_sel) == ICON_BYTES
    header[OFF_ICON_UNSEL:OFF_ICON_UNSEL + ICON_BYTES] = icon_unsel
    header[OFF_ICON_SEL:OFF_ICON_SEL + ICON_BYTES] = icon_sel

    # Checksum: sum of every byte with both checksum fields set to zero.
    struct.pack_into(">I", header, OFF_CKSUM, 0)
    cksum = (byte_sum(header) + byte_sum(code)) & 0xFFFFFFFF
    struct.pack_into(">I", header, OFF_CKSUM, cksum)

    out = bytes(header) + bytes(code) + struct.pack(">I", cksum)
    check_g3a(out, "the output")  # belt and braces
    return out


def name_from_path(path: str) -> str:
    stem = os.path.splitext(os.path.basename(path))[0]
    return stem.replace("_", " ").replace("-", " ").strip().title() or "Python"


def convert(py_path: str, out_path: str | None = None, name: str | None = None,
            icon: str | None = None, icon_selected: str | None = None,
            runner_path: str | None = None) -> str:
    """Convert one file. Returns the output path."""
    runner_path = runner_path or default_runner_path()
    if not os.path.isfile(runner_path):
        raise ConvertError(f"can't find runner.g3a (looked for {runner_path}). Keep it next to py2g3a.py.")
    if not os.path.isfile(py_path):
        raise ConvertError(f"can't find {py_path}")
    if out_path is None:
        out_path = os.path.splitext(py_path)[0] + ".g3a"
    name = name or name_from_path(py_path)

    unsel = load_icon(icon) if icon else None
    sel = load_icon(icon_selected) if icon_selected else unsel

    with open(runner_path, "rb") as f:
        runner = f.read()
    with open(py_path, "rb") as f:
        script = f.read()

    data = build_g3a(runner, script, name, out_path, unsel, sel)
    with open(out_path, "wb") as f:
        f.write(data)
    return out_path


# --------------------------------------------------------------------------
# Command line and (optional) mini window
# --------------------------------------------------------------------------

def run_gui() -> int:
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox, simpledialog
    except ImportError:
        print(__doc__)
        return 1
    root = tk.Tk()
    root.withdraw()
    paths = filedialog.askopenfilenames(title="Choose Python script(s) to convert",
                                        filetypes=[("Python scripts", "*.py"), ("All files", "*.*")])
    if not paths:
        return 0
    made, errors = [], []
    for p in paths:
        default = name_from_path(p)
        name = default if len(paths) > 1 else simpledialog.askstring(
            "py2g3a", "Name shown in the calculator's menu:", initialvalue=default, parent=root)
        if name is None:
            return 0
        try:
            made.append(convert(p, name=name or default))
        except ConvertError as e:
            errors.append(f"{os.path.basename(p)}: {e}")
    if made:
        messagebox.showinfo("py2g3a", "Made:\n" + "\n".join(made) +
                            "\n\nCopy to your calculator over USB (USB Flash mode).")
    if errors:
        messagebox.showerror("py2g3a", "\n".join(errors))
    return 1 if errors else 0


def main(argv=None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if not argv:
        return run_gui()
    ap = argparse.ArgumentParser(
        prog="py2g3a",
        description="Turn Python scripts into Casio fx-CG10/20/50 add-ins (.g3a) with their own menu icon.")
    ap.add_argument("scripts", nargs="+", help="the .py file(s) to convert")
    ap.add_argument("-o", "--output", help="output .g3a path (only with a single script)")
    ap.add_argument("-n", "--name", help="name shown in MAIN MENU (default: from the file name)")
    ap.add_argument("-i", "--icon", help="PNG for the menu icon (scaled to 92x64)")
    ap.add_argument("-s", "--icon-selected", help="PNG for the icon when highlighted (default: same as --icon)")
    ap.add_argument("-r", "--runner", help="path to runner.g3a (default: next to this script)")
    ap.add_argument("--version", action="version", version=f"py2g3a {__version__}")
    args = ap.parse_args(argv)

    if len(args.scripts) > 1 and (args.output or args.name):
        ap.error("--output and --name only work with a single script")

    failed = False
    for path in args.scripts:
        try:
            out = convert(path, args.output, args.name, args.icon, args.icon_selected, args.runner)
            print(f"made {out}  ({os.path.getsize(out) // 1024} KB)")
        except ConvertError as e:
            print(f"py2g3a: {path}: {e}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
