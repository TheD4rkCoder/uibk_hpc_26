"""One-off extraction of the chart + table data that was trapped in the original
PowerPoint deck.  Run once; the CSVs it writes under figs/data are the source of
truth for the generated charts from then on.

    python figs/scripts/extract_data.py path/to/02_mpi_basics.pptx
"""
import csv
import os
import re
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "data"))


def num(s):
    """'1,80' -> 1.80 ; '' -> None"""
    s = (s or "").strip().replace(" ", "")
    if not s:
        return None
    return float(s.replace(".", "").replace(",", ".")) if "," in s else float(s)


def extract_charts(z):
    """Latency + L3-miss series, from the cached values in the chart parts."""
    x = z.read("ppt/charts/chart2.xml").decode("utf8")
    series = re.findall(r"<c:ser>.*?</c:ser>", x, re.S)
    rows, names = {}, []
    cats = None
    for s in series:
        name = re.search(r"<c:tx>.*?<c:v>(.*?)</c:v>", s, re.S).group(1)
        names.append(name)
        if cats is None:
            cm = re.search(r"<c:cat>(.*?)</c:cat>", s, re.S)
            if cm:
                cats = re.findall(r"<c:v>([^<]*)</c:v>", cm.group(1))
        vm = re.search(r"<c:val>(.*?)</c:val>", s, re.S)
        rows[name] = [float(v) for v in re.findall(r"<c:v>([^<]*)</c:v>", vm.group(1))]

    out = os.path.join(DATA, "osu_latency.csv")
    with open(out, "w", newline="", encoding="utf8") as fh:
        w = csv.writer(fh)
        w.writerow(["problem_size"] + names)
        for i, c in enumerate(cats):
            w.writerow([c] + [rows[n][i] for n in names])
    print("wrote", out, "->", len(cats), "rows x", len(names), "series")
    print("   series:", names)


def extract_table(z):
    """The heat-mapped launcher x MPI-build latency table on the WIP slide."""
    x = z.read("ppt/slides/slide51.xml").decode("utf8")
    trs = re.findall(r"<a:tr .*?</a:tr>", x, re.S)
    grid = []
    for tr in trs:
        cells = []
        for tc in re.findall(r"<a:tc[ >].*?</a:tc>", tr, re.S):
            cells.append("".join(re.findall(r"<a:t>([^<]*)</a:t>", tc)).strip())
        grid.append(cells)

    launchers, builds = grid[0][1:], grid[1][1:]
    # the launcher name only appears above its first column; fill to the right
    filled, last = [], ""
    for v in launchers:
        last = v or last
        filled.append(last)

    out = os.path.join(DATA, "osu_launchers.csv")
    with open(out, "w", newline="", encoding="utf8") as fh:
        w = csv.writer(fh)
        w.writerow(["launcher", "mpi_build", "message_size", "latency_us"])
        n = 0
        for row in grid[2:]:
            if not row or not row[0].strip():
                continue
            size = row[0].strip()
            for launcher, build, cell in zip(filled, builds, row[1:]):
                v = num(cell)
                if v is None:
                    continue
                w.writerow([launcher, build, size, v])
                n += 1
    print("wrote", out, "->", n, "measurements")
    print("   launchers:", sorted(set(filled)))
    print("   builds:", len(set(builds)))


def main():
    pptx = sys.argv[1]
    os.makedirs(DATA, exist_ok=True)
    with zipfile.ZipFile(pptx) as z:
        extract_charts(z)
        extract_table(z)


if __name__ == "__main__":
    main()
