"""Regenerate the heat-mapped launcher/MPI-build latency table.

Replaces the PowerPoint table object on the WIP slide with a real HTML table
built from figs/data/osu_launchers.csv, so the numbers stay searchable,
selectable and re-generatable instead of being pixels.

Conditional formatting is applied *per row* -- i.e. comparing MPI builds at the
same message size -- which is what the original workbook did.
"""
import csv
import os
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "data", "osu_launchers.csv"))

# NOTE: this one lands in figs/, not figs/out/, and is meant to be committed.
# Quarto resolves {{< include >}} directives *before* it runs the pre-render
# hook, so an include target that lives in the disposable figs/out/ cache makes
# a clean checkout fail to render. Keeping it outside that cache avoids this.
OUT = os.path.normpath(os.path.join(HERE, ".."))

INK = "#10243A"        # text on top of the heat colours -- same in both themes
# The heat cells bring their own background, so only the headers follow the
# deck theme. This table ends up as inline HTML in the slide, so it can simply
# read the CSS custom properties that shared/uibk-light.scss redefines for the
# light theme; the fallbacks are the dark values.
FG = "var(--fig-fg, #F2F4F7)"
MUTED = "var(--fig-muted, #9FB0C3)"
RULE = "var(--fig-rule, #2A3340)"

# green -> yellow -> red, chosen to stay legible on a dark slide
STOPS = [(0.0, (0x3F, 0xA0, 0x6A)), (0.5, (0xE3, 0xC2, 0x4B)), (1.0, (0xD9, 0x5F, 0x55))]


def heat(t):
    t = 0.0 if t < 0 else (1.0 if t > 1 else t)
    for (t0, c0), (t1, c1) in zip(STOPS, STOPS[1:]):
        if t <= t1:
            f = 0.0 if t1 == t0 else (t - t0) / (t1 - t0)
            return tuple(round(a + (b - a) * f) for a, b in zip(c0, c1))
    return STOPS[-1][1]


def short(build):
    """intel-mpi_2019.10.317-gcc-8.5.0 -> intel-mpi 2019 / gcc-8.5.0"""
    fam, _, rest = build.partition("_")
    ver, _, comp = rest.partition("-")
    return "%s %s<br>%s" % (fam, ver.split(".")[0], comp)


def main():
    os.makedirs(OUT, exist_ok=True)
    with open(DATA, encoding="utf8") as fh:
        rows = list(csv.DictReader(fh))

    launchers = list(OrderedDict.fromkeys(r["launcher"] for r in rows))
    builds = list(OrderedDict.fromkeys(r["mpi_build"] for r in rows))
    sizes = list(OrderedDict.fromkeys(r["message_size"] for r in rows))
    sizes.sort(key=lambda s: int(s))

    cell = {(r["launcher"], r["mpi_build"], r["message_size"]): float(r["latency_us"])
            for r in rows}
    cols = [(l, b) for l in launchers for b in builds]

    out = []
    out.append('<div class="osu-table">')
    out.append("<table>")

    # two header rows: launcher groups, then the individual builds
    out.append("<thead><tr><th></th>")
    for l in launchers:
        out.append('<th colspan="%d" class="grp">%s</th>' % (len(builds), l))
    out.append("</tr><tr><th class=\"sz\">bytes</th>")
    for _, b in cols:
        out.append('<th class="bld">%s</th>' % short(b))
    out.append("</tr></thead><tbody>")

    for s in sizes:
        vals = [cell.get((l, b, s)) for l, b in cols]
        present = [v for v in vals if v is not None]
        lo, hi = (min(present), max(present)) if present else (0, 1)
        span = (hi - lo) or 1.0
        out.append('<tr><th class="sz">%s</th>' % s)
        for v in vals:
            if v is None:
                out.append('<td class="na"></td>')
                continue
            r, g, b_ = heat((v - lo) / span)
            out.append('<td style="background:rgb(%d,%d,%d)">%s</td>'
                       % (r, g, b_, ("%.2f" % v).replace(".", ",")))
        out.append("</tr>")

    out.append("</tbody></table></div>")

    css = """
<style>
.osu-table{overflow-x:auto;font-variant-numeric:tabular-nums}
.osu-table table{border-collapse:collapse;font-size:0.30em;margin:0 auto}
.osu-table th,.osu-table td{padding:1px 4px;text-align:right;white-space:nowrap}
.osu-table td{color:%s}
.osu-table th.grp{color:%s;font-weight:600;text-align:center;
  border-bottom:1px solid %s;padding-bottom:2px}
.osu-table th.bld{color:%s;font-weight:400;text-align:right;
  vertical-align:bottom;line-height:1.15;padding-bottom:3px}
.osu-table th.sz{color:%s;text-align:right;padding-right:6px}
.osu-table td.na{background:transparent}
</style>
""" % (INK, FG, RULE, MUTED, FG)

    path = os.path.join(OUT, "s51_table.md")
    with open(path, "w", encoding="utf8") as fh:
        fh.write(css + "\n" + "".join(out) + "\n")
    print("wrote", path, "->", len(sizes), "rows x", len(cols), "columns")


if __name__ == "__main__":
    main()
