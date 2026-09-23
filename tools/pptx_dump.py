"""Inspect a PowerPoint deck so it can be converted to Quarto by hand.

    python tools/pptx_dump.py <deck>.pptx            # outline of every slide
    python tools/pptx_dump.py <deck>.pptx --shapes   # + shape inventory
    python tools/pptx_dump.py <deck>.pptx --media DIR  # extract embedded media
    python tools/pptx_dump.py <deck>.pptx -s 21      # only slide 21, in detail

The point of this tool is to answer, per slide: is this text (-> Markdown), a
diagram built from native shapes (-> redraw as TikZ/Graphviz), a real picture
or screenshot (-> keep as an image), a chart (-> regenerate from its data), or
a table (-> a real Markdown/HTML table)?
"""
import argparse
import os
import re
import shutil
import sys
import zipfile
from collections import Counter

EMU = 914400.0

# Slide text is full of typographic quotes, dashes and Greek letters that the
# Windows console codepage cannot represent.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except AttributeError:  # pragma: no cover
    pass


def slide_numbers(z):
    ns = []
    for n in z.namelist():
        m = re.fullmatch(r"ppt/slides/slide(\d+)\.xml", n)
        if m:
            ns.append(int(m.group(1)))
    return sorted(ns)


def rels(z, i):
    name = "ppt/slides/_rels/slide%d.xml.rels" % i
    if name not in z.namelist():
        return {}
    x = z.read(name).decode("utf8")
    return dict(re.findall(r'Id="([^"]+)"[^>]*Target="([^"]+)"', x))


def paragraphs(blob):
    """(indent level, text) for each non-empty paragraph, in document order."""
    out = []
    for p in re.findall(r"<a:p>.*?</a:p>", blob, re.S):
        lvl = re.search(r'<a:pPr[^>]*lvl="(\d+)"', p)
        txt = "".join(re.findall(r"<a:t>([^<]*)</a:t>", p)).strip()
        if txt:
            out.append((int(lvl.group(1)) if lvl else 0, txt))
    return out


def shape_blocks(x):
    return re.findall(r"<p:(sp|pic|cxnSp|graphicFrame)>.*?</p:\1>", x, re.S)


def dump_slide(z, i, show_shapes, detail):
    x = z.read("ppt/slides/slide%d.xml" % i).decode("utf8")
    rel = rels(z, i)
    media = sorted({os.path.basename(v) for v in rel.values() if "/media/" in v})

    sp = len(re.findall(r"<p:sp>", x))
    pic = len(re.findall(r"<p:pic>", x))
    cxn = len(re.findall(r"<p:cxnSp>", x))
    tbl = len(re.findall(r"<a:tbl>", x))
    chart = len([v for v in rel.values() if "/charts/" in v])
    geoms = Counter(re.findall(r'prst="([a-zA-Z0-9]+)"', x))

    kind = []
    if sp + cxn > 8 and pic == 0:
        kind.append("SHAPE-DIAGRAM")
    if pic:
        kind.append("PICTURES(%d)" % pic)
    if chart:
        kind.append("CHART")
    if tbl:
        kind.append("TABLE")
    if media and any(m.lower().endswith((".mp4", ".avi", ".mov")) for m in media):
        kind.append("VIDEO")
    if not kind:
        kind.append("text")

    print("=" * 78)
    print("SLIDE %-3d  %s" % (i, " ".join(kind)))
    print("  counts: sp=%d pic=%d cxn=%d chart=%d tbl=%d" % (sp, pic, cxn, chart, tbl))
    if geoms:
        print("  geoms : %s" % dict(geoms.most_common(8)))
    if media:
        print("  media : %s" % ", ".join(media))
    print("-" * 78)
    for lvl, t in paragraphs(x):
        print("   " + "  " * lvl + ("- " if lvl else "") + t)

    if tbl:
        print("  --- table ---")
        for tr in re.findall(r"<a:tr .*?</a:tr>", x, re.S):
            cells = ["".join(re.findall(r"<a:t>([^<]*)</a:t>", tc)).strip()
                     for tc in re.findall(r"<a:tc[ >].*?</a:tc>", tr, re.S)]
            print("   | " + " | ".join(c or " " for c in cells))

    if show_shapes or detail:
        print("  --- shapes (inches) ---")
        for blob in shape_blocks(x):
            prst = re.search(r'prst="([^"]*)"', blob)
            off = re.search(r'<a:off x="(-?\d+)" y="(-?\d+)"/>', blob)
            ext = re.search(r'<a:ext cx="(\d+)" cy="(\d+)"/>', blob)
            fill = re.search(r"<a:solidFill><a:srgbClr val=\"([0-9A-Fa-f]{6})\"", blob)
            t = " ".join(re.findall(r"<a:t>([^<]*)</a:t>", blob)).strip()
            pos = ""
            if off and ext:
                pos = "x=%5.2f y=%5.2f w=%5.2f h=%5.2f" % (
                    int(off.group(1)) / EMU, int(off.group(2)) / EMU,
                    int(ext.group(1)) / EMU, int(ext.group(2)) / EMU)
            print("    %-16s %-38s fill=%-7s %s" % (
                prst.group(1) if prst else "-", pos,
                fill.group(1) if fill else "-", repr(t[:40]) if t else ""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pptx")
    ap.add_argument("-s", "--slide", type=int, action="append")
    ap.add_argument("--shapes", action="store_true")
    ap.add_argument("--media", metavar="DIR")
    a = ap.parse_args()

    with zipfile.ZipFile(a.pptx) as z:
        if a.media:
            os.makedirs(a.media, exist_ok=True)
            n = 0
            for name in z.namelist():
                if name.startswith("ppt/media/"):
                    with z.open(name) as src, \
                         open(os.path.join(a.media, os.path.basename(name)), "wb") as dst:
                        shutil.copyfileobj(src, dst)
                    n += 1
            print("extracted %d media files to %s" % (n, a.media))
            return
        for i in (a.slide or slide_numbers(z)):
            dump_slide(z, i, a.shapes, bool(a.slide))


if __name__ == "__main__":
    main()
