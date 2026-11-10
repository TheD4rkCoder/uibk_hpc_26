#!/usr/bin/env python3
"""Generate the explanatory figures in docs/ (run from the project root)."""

import os
import tempfile

os.environ.setdefault("MPLCONFIGDIR", tempfile.mkdtemp(prefix="mpl-"))

import matplotlib.pyplot as plt
import numpy as np

DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs")
os.makedirs(DOCS, exist_ok=True)

ORANGE = "#dd8452"
BLUE = "#4c72b0"
GREEN = "#55a868"


def opening_criterion_figure():
    fig, (schematic, tree_axes) = plt.subplots(1, 2, figsize=(11, 5))

    # --- geometry of the criterion -------------------------------------
    size = 1.0
    theta = 0.5
    center_of_mass = np.array([0.5, 0.5])
    schematic.add_patch(plt.Rectangle((0, 0), size, size, fill=False, lw=2))
    schematic.text(0.5, -0.12, "cell, width s", ha="center")

    reach = size / theta
    targets = [
        (np.array([1.39, 0.63]), BLUE, "opened\n(d < s/theta)", (-0.12, -0.42)),
        (np.array([3.05, 1.39]), ORANGE, "approximated\n(d > s/theta)", (0.10, 0.18)),
    ]
    for target, colour, label, offset in targets:
        schematic.plot(*center_of_mass, "k+", ms=14)
        schematic.plot(*target, "o", color=colour, ms=7)
        schematic.annotate("", xy=target, xytext=center_of_mass,
                           arrowprops=dict(arrowstyle="->", color=colour, lw=1.5, alpha=0.6))
        schematic.text(target[0] + offset[0], target[1] + offset[1], label, color=colour,
                       fontsize=9, ha="left")
    schematic.add_patch(plt.Circle(center_of_mass, reach, fill=False, ls="--", color="grey"))
    schematic.text(center_of_mass[0] + reach * 0.35, center_of_mass[1] + reach * 0.93,
                   "distance s/theta", color="grey", fontsize=9)
    schematic.set_xlim(-0.3, 3.6)
    schematic.set_ylim(-0.4, 2.6)
    schematic.set_aspect("equal")
    schematic.axis("off")
    schematic.set_title("opening criterion (one cell and two targets)")

    # --- a real quadtree with the accepted cells for one target ---------
    random = np.random.default_rng(1)
    points = np.concatenate([
        random.normal([0.35, 0.30], 0.10, (180, 2)),
        random.normal([0.70, 0.65], 0.13, (220, 2)),
    ])
    target = np.array([0.80, 0.78])

    leaf_capacity = 8

    def build(x0, y0, width, indices, depth):
        node = {"x0": x0, "y0": y0, "width": width, "indices": indices, "children": None}
        if len(indices) <= leaf_capacity or depth >= 8 or width < 1e-3:
            return node
        mid_x, mid_y = x0 + width / 2, y0 + width / 2
        quadrants = [[], [], [], []]
        for index in indices:
            quadrant = (points[index, 0] > mid_x) + 2 * (points[index, 1] > mid_y)
            quadrants[quadrant].append(index)
        node["children"] = [
            build(x0 + (q % 2) * width / 2, y0 + (q // 2) * width / 2, width / 2, quadrants[q],
                  depth + 1)
            for q in range(4) if quadrants[q]
        ]
        return node

    def center_of_mass_of(node):
        return points[node["indices"]].mean(axis=0)

    root = build(0.0, 0.0, 1.0, list(range(len(points))), 0)

    accepted = []
    opened_leaves = []

    def classify(node):
        com = center_of_mass_of(node)
        if np.linalg.norm(target - com) > node["width"] / theta:
            accepted.append(node)
            return
        if node["children"] is None:
            opened_leaves.append(node)
            return
        for child in node["children"]:
            classify(child)

    classify(root)

    tree_axes.scatter(points[:, 0], points[:, 1], s=6, color="0.55")
    for node in accepted:
        tree_axes.add_patch(plt.Rectangle((node["x0"], node["y0"]), node["width"],
                                          node["width"], facecolor=ORANGE, alpha=0.18,
                                          edgecolor=ORANGE, lw=0.8))
    for node in opened_leaves:
        tree_axes.add_patch(plt.Rectangle((node["x0"], node["y0"]), node["width"],
                                          node["width"], facecolor="none",
                                          edgecolor=BLUE, lw=0.8))
    tree_axes.plot(*target, "o", color="k", ms=7, label="target")
    tree_axes.set_xlim(0, 1)
    tree_axes.set_ylim(0, 1)
    tree_axes.set_aspect("equal")
    tree_axes.set_title("2D quadtree, theta = 0.5: orange cells are one multipole")
    tree_axes.legend(loc="lower right")

    fig.tight_layout()
    fig.savefig(os.path.join(DOCS, "opening_criterion.png"), dpi=150)
    plt.close(fig)


def quadrupole_figure():
    fig, (cell_axes, error_axes) = plt.subplots(1, 2, figsize=(11, 5))

    # --- lopsided cell: monopole and quadrupole describe different things
    masses = [(-0.30, -0.10, 1.0), (-0.18, 0.22, 1.0), (-0.33, 0.12, 1.0), (0.35, 0.10, 0.2)]
    total_mass = sum(m for _, _, m in masses)
    center_of_mass = np.array([sum(m * x for x, _, m in masses),
                               sum(m * y for _, y, m in masses)]) / total_mass

    def exact(target):
        acc = np.zeros(2)
        for x, y, m in masses:
            delta = np.array([x, y]) - target
            acc += m * delta / np.linalg.norm(delta) ** 3
        return acc

    cell_axes.add_patch(plt.Rectangle((-0.5, -0.5), 1, 1, fill=False, lw=2))
    for x, y, m in masses:
        cell_axes.plot(x, y, "o", ms=5 + 12 * m, color="0.35")
    cell_axes.plot(*center_of_mass, "k+", ms=14)
    cell_axes.annotate("centre of mass", center_of_mass, xytext=(0.65, 0.62), fontsize=8,
                       arrowprops=dict(arrowstyle="->", lw=1, color="k"))
    cell_axes.text(-0.5, -0.60,
                   "monopole: total mass at +\nquadrupole: + shape (2nd moment)",
                   fontsize=8.5, va="top")
    cell_axes.set_xlim(-0.7, 2.0)
    cell_axes.set_ylim(-0.95, 1.0)
    cell_axes.set_aspect("equal")
    cell_axes.axis("off")
    cell_axes.set_title("monopole keeps only the total mass and its centre")

    def monopole_plus_quadrupole(target):
        r = np.array([target[0] - center_of_mass[0], target[1] - center_of_mass[1], 0.0])
        distance = np.linalg.norm(r)
        acceleration = -total_mass * r / distance ** 3
        quadrupole = np.zeros((3, 3))
        for x, y, m in masses:
            offset = np.array([x - center_of_mass[0], y - center_of_mass[1], 0.0])
            quadrupole += m * (3 * np.outer(offset, offset) - np.dot(offset, offset) * np.eye(3))
        qr = quadrupole @ r
        acceleration += qr / distance ** 5
        acceleration += -2.5 * (r @ qr) * r / distance ** 7
        return acceleration[:2]

    # --- error versus distance for monopole and monopole+quadrupole
    distances = np.linspace(1.2, 20, 200)
    monopole_errors = []
    quadrupole_errors = []
    for distance in distances:
        point = np.array([distance, 0.0])
        truth = exact(point)
        quadr = monopole_plus_quadrupole(point)
        monopole_errors.append(np.linalg.norm(-total_mass * (point - center_of_mass) /
                                              np.linalg.norm(point - center_of_mass) ** 3 - truth) /
                               np.linalg.norm(truth))
        quadrupole_errors.append(np.linalg.norm(quadr - truth) / np.linalg.norm(truth))

    error_axes.loglog(distances, monopole_errors, label="monopole only", color=BLUE)
    error_axes.loglog(distances, quadrupole_errors, label="monopole + quadrupole", color=ORANGE)
    error_axes.set_xlabel("distance from the cell, in cell widths")
    error_axes.set_ylabel("relative force error")
    error_axes.grid(True, which="both", alpha=0.3)
    error_axes.legend()
    error_axes.set_title("the quadrupole removes the leading error term")

    fig.tight_layout()
    fig.savefig(os.path.join(DOCS, "quadrupole.png"), dpi=150)
    plt.close(fig)


def theta_tradeoff_figure():
    # From ./validate and ./validate_quad, N = 2000 cube, softening = 0.
    theta = [0.2, 0.5, 0.8, 1.0, 2.0, 100.0]
    interactions = [1223.0, 319.3, 115.3, 67.7, 24.9, 24.8]
    monopole_error = [1.055e-4, 1.945e-3, 8.428e-3, 1.706e-2, 9.412e-2, 1.025e-1]
    quadrupole_error = [1.094e-5, 5.222e-4, 4.081e-3, 1.100e-2, 8.999e-2, 1.219e-1]

    fig, axes = plt.subplots(figsize=(6.5, 4.5))
    axes.loglog(interactions, monopole_error, "o-", color=BLUE, label="monopole")
    axes.loglog(interactions, quadrupole_error, "s-", color=ORANGE, label="quadrupole")
    for x, y, label, offset in zip(interactions, monopole_error, theta,
                                   [(4, 6), (4, 6), (4, -12), (-30, 8), (4, 6), (-34, -14)]):
        axes.annotate(f"theta={label:g}", (x, y), textcoords="offset points",
                      xytext=offset, fontsize=8, color=BLUE)
    axes.set_xlim(20, 2600)
    axes.set_xlabel("interactions per particle (cost)")
    axes.set_ylabel("relative L2 force error")
    axes.grid(True, which="both", alpha=0.3)
    axes.legend()
    axes.set_title("accuracy versus work: opening angle sets the tradeoff")
    fig.tight_layout()
    fig.savefig(os.path.join(DOCS, "theta_tradeoff.png"), dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    opening_criterion_figure()
    quadrupole_figure()
    theta_tradeoff_figure()
    print("wrote docs/opening_criterion.png, docs/quadrupole.png, docs/theta_tradeoff.png")
