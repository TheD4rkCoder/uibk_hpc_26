"""Rebuild the clip-art compositions of this deck as single transparent PNGs.

Several slides illustrate a point by arranging the same few pieces of clip art
(apple trees, workers, a barn) in different ways -- that arrangement *is* the
content, so it has to be preserved. The pieces themselves are drawings that
cannot sensibly be redrawn as TikZ, so the rule "diagrams get redrawn, pictures
stay pictures" puts them here.

Rather than screenshotting the slide (which would bake in a white background
and wreck the dark theme), this reads each picture's position out of the .pptx
and composites the original, transparent assets onto a transparent canvas. The
result drops onto a black slide with no halo.

Handling the real .pptx geometry means dealing with:
  * nested <p:grpSp> groups, whose children live in their own coordinate space
    and must be mapped through the group's off/ext vs. chOff/chExt, and
  * <a:srcRect>, which crops the source bitmap before it is scaled.

This is a ONE-OFF extraction, not part of the build: it needs the original
.pptx, which is not kept in the repository. Its outputs are committed to
img/ instead, exactly like the CSVs that extract_data.py produces for
lecture 02. Re-run it only if the compositions have to change:

    python figs/scripts/extract_compositions.py path/to/01_....pptx
"""
import os
import sys
import zipfile
from io import BytesIO
from xml.etree import ElementTree as ET

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
DECK = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.normpath(os.path.join(DECK, "img"))

NS = {
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "p": "http://schemas.openxmlformats.org/presentationml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
}
R_EMBED = "{%s}embed" % NS["r"]

EMU_PER_IN = 914400.0
DPI = 150.0
NAVY = (0, 51, 97, 255)

SLIDES = {
    29: "s29_types",
    30: "s30_data_parallel",
    31: "s31_data_parallel_trees",
    32: "s32_task_parallel",
    33: "s33_hybrid",
}


def px(emu):
    return int(round(emu / EMU_PER_IN * DPI))


def local(tag):
    return tag.rsplit("}", 1)[-1]


def xfrm_of(el, path):
    x = el.find(path, NS)
    return x


def read_xfrm(x):
    off = x.find("a:off", NS)
    ext = x.find("a:ext", NS)
    if off is None or ext is None:
        return None
    return (int(off.get("x")), int(off.get("y")),
            int(ext.get("cx")), int(ext.get("cy")),
            x.get("flipH") == "1", x.get("flipV") == "1")


# A transform maps child coordinates to slide coordinates:
#   X = ax + x * sx        W = w * sx
IDENT = (0.0, 0.0, 1.0, 1.0)


def compose(parent, child):
    ax, ay, sx, sy = parent
    bx, by, tx, ty = child
    return (ax + bx * sx, ay + by * sy, sx * tx, sy * ty)


def group_transform(grp):
    x = grp.find("p:grpSpPr/a:xfrm", NS)
    if x is None:
        return IDENT
    off = x.find("a:off", NS)
    ext = x.find("a:ext", NS)
    choff = x.find("a:chOff", NS)
    chext = x.find("a:chExt", NS)
    if None in (off, ext, choff, chext):
        return IDENT
    cx, cy = int(chext.get("cx")) or 1, int(chext.get("cy")) or 1
    sx = int(ext.get("cx")) / cx
    sy = int(ext.get("cy")) / cy
    ax = int(off.get("x")) - int(choff.get("x")) * sx
    ay = int(off.get("y")) - int(choff.get("y")) * sy
    return (ax, ay, sx, sy)


def apply(t, x, y, w, h):
    ax, ay, sx, sy = t
    return (ax + x * sx, ay + y * sy, w * sx, h * sy)


def crop_rect(pic):
    sr = pic.find("p:blipFill/a:srcRect", NS)
    if sr is None:
        return None
    def f(k):
        return int(sr.get(k, "0")) / 100000.0
    return f("l"), f("t"), f("r"), f("b")


def collect(node, t, rel, out):
    """Walk the shape tree, flattening groups into slide coordinates."""
    for child in node:
        tag = local(child.tag)
        if tag == "grpSp":
            collect(child, compose(t, group_transform(child)), rel, out)
        elif tag == "pic":
            x = child.find("p:spPr/a:xfrm", NS)
            blip = child.find("p:blipFill/a:blip", NS)
            if x is None or blip is None:
                continue
            v = read_xfrm(x)
            if not v:
                continue
            eid = blip.get(R_EMBED)
            if eid not in rel:
                continue
            gx, gy, gw, gh = apply(t, v[0], v[1], v[2], v[3])
            out.append(dict(kind="pic", x=gx, y=gy, w=gw, h=gh,
                            fliph=v[4], flipv=v[5],
                            media="ppt/" + rel[eid].replace("../", ""),
                            crop=crop_rect(child)))
        elif tag == "cxnSp":
            prst = child.find("p:spPr/a:prstGeom", NS)
            if prst is None or "straightConnector" not in prst.get("prst", ""):
                continue
            x = child.find("p:spPr/a:xfrm", NS)
            v = read_xfrm(x) if x is not None else None
            if not v:
                continue
            gx, gy, gw, gh = apply(t, v[0], v[1], v[2], v[3])
            out.append(dict(kind="arrow", x=gx, y=gy, w=gw, h=gh,
                            fliph=v[4], flipv=v[5]))


def drop_white_background(im):
    """Make the white *background* transparent, without punching holes in white
    parts of the drawing itself: flood-fill inwards from the border, so only
    white connected to the edge is removed. A picture whose border is not white
    (the barn's sky, a photograph) is left completely alone.
    """
    import numpy as np
    from collections import deque

    a = np.asarray(im).copy()
    h, w = a.shape[:2]
    light = (a[..., 0] > 240) & (a[..., 1] > 240) & (a[..., 2] > 240)
    seen = np.zeros((h, w), bool)
    q = deque()
    for x in range(w):
        for y in (0, h - 1):
            if light[y, x] and not seen[y, x]:
                seen[y, x] = True
                q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if light[y, x] and not seen[y, x]:
                seen[y, x] = True
                q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and light[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                q.append((ny, nx))
    a[..., 3][seen] = 0
    return Image.fromarray(a, "RGBA")


def load_image(z, item):
    im = Image.open(BytesIO(z.read(item["media"]))).convert("RGBA")
    # Clip art exported without transparency carries a white box that would
    # glare on a black slide. Only strip it when the image really is fully
    # opaque -- never touch something that already has an alpha channel.
    if im.getchannel("A").getextrema()[0] == 255:
        im = drop_white_background(im)
    if item["crop"]:
        l, t_, r_, b_ = item["crop"]
        w, h = im.size
        im = im.crop((int(w * l), int(h * t_),
                      int(w * (1 - r_)), int(h * (1 - b_))))
    return im


def build(z, n, name, rels):
    root = ET.fromstring(z.read("ppt/slides/slide%d.xml" % n))
    tree = root.find("p:cSld/p:spTree", NS)
    items = []
    collect(tree, IDENT, rels, items)
    if not items:
        return None

    x0 = min(i["x"] for i in items)
    y0 = min(i["y"] for i in items)
    x1 = max(i["x"] + i["w"] for i in items)
    y1 = max(i["y"] + i["h"] for i in items)

    canvas = Image.new("RGBA", (px(x1 - x0) or 1, px(y1 - y0) or 1), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)

    for it in items:
        left, top = px(it["x"] - x0), px(it["y"] - y0)
        w, h = max(1, px(it["w"])), max(1, px(it["h"]))
        if it["kind"] == "pic":
            im = load_image(z, it)
            if it["fliph"]:
                im = im.transpose(Image.FLIP_LEFT_RIGHT)
            if it["flipv"]:
                im = im.transpose(Image.FLIP_TOP_BOTTOM)
            canvas.alpha_composite(im.resize((w, h), Image.LANCZOS), (left, top))
        else:
            y = top + h // 2
            xa, xb = (left + w, left) if it["fliph"] else (left, left + w)
            draw.line([(xa, y), (xb, y)], fill=NAVY, width=max(2, px(38100)))
            d = 1 if xb > xa else -1
            head = max(6, px(150000))
            draw.polygon([(xb, y), (xb - d * head, y - head // 2),
                          (xb - d * head, y + head // 2)], fill=NAVY)

    path = os.path.join(OUT, name + ".png")
    canvas.save(path)
    return path, canvas.size


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__.strip().splitlines()[-1].strip())
    os.makedirs(OUT, exist_ok=True)
    with zipfile.ZipFile(sys.argv[1]) as z:
        for n, name in sorted(SLIDES.items()):
            rx = ET.fromstring(z.read("ppt/slides/_rels/slide%d.xml.rels" % n))
            rel = {e.get("Id"): e.get("Target") for e in rx}
            res = build(z, n, name, rel)
            if res:
                print("wrote %-28s %dx%d" % (os.path.basename(res[0]), *res[1]))


if __name__ == "__main__":
    main()
