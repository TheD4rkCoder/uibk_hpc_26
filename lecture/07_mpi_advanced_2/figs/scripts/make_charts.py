"""Regenerate the send/recv vs. MPI_Put comparison of slide 25.

Two measured numbers, so they live here as data rather than as a formula:
10^8 ints from rank 0 to rank 1 on LCC2 (openmpi/3.1.1, two ranks, one per
node), once with plain send/recv calls and once with MPI_Put plus fence
synchronisation. 166.577 / 73.671 = 2.26x, which is the figure quoted on the
slide.

The chart is written twice: the dark variant into figs/out/ and the light one
into figs/out/light/ (see shared/figs/figtheme.py).
"""
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from figtheme import themes  # shared/figs, put on PYTHONPATH by build_figs.py

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "out"))

MEASUREMENTS = [("Send/Recv", 166.577, "blu"), ("MPI_Put()", 73.671, "orange")]


def chart(t, path):
    labels = [m[0] for m in MEASUREMENTS]
    values = [m[1] for m in MEASUREMENTS]
    colours = [getattr(t, m[2]) for m in MEASUREMENTS]

    fig, ax = plt.subplots(figsize=(5.2, 4.4))
    bars = ax.bar(labels, values, color=colours, width=0.55)
    for b, v in zip(bars, values):
        ax.text(b.get_x() + b.get_width() / 2, v + 3, "%.1f" % v,
                ha="center", va="bottom", color=t.fg, fontsize=13)
    ax.set_ylabel("execution time [s]")
    ax.set_ylim(0, 190)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(axis="y", color=t.grid, linewidth=0.8)
    ax.set_axisbelow(True)
    fig.tight_layout()
    fig.savefig(path, format="svg", transparent=True, bbox_inches="tight")
    plt.close(fig)
    print("wrote", os.path.join(os.path.basename(os.path.dirname(path)),
                                os.path.basename(path)))


def main():
    for t in themes(OUT):
        # the category labels sit on the slide background, not on the bars
        t.apply(**{"font.size": 14, "xtick.color": t.fg})
        chart(t, t.out("s25_onesided_perf.svg"))


if __name__ == "__main__":
    main()
