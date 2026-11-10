#!/usr/bin/env python3
"""Real-time / replay viewer for the n-body frames.

Live from the cluster (no files written). The simulation must run on a compute
node, never on the login node; srun forwards the frames over the ssh pipe:

    ssh -F ~/.ssh/config login.lcc3.uibk.ac.at \
        'cd ~/uibk_hpc_26/proseminar/05/david && \
         srun -u -p lva -n 1 --time=00:30:00 \
         ./nbody --solver bh --ic clusters --n 5000 --steps 500 --theta 0.5 --emit --quiet' \
        | python3 tools/viewer.py

Replay a recorded frame file, or save a gif for the report:

    python3 tools/viewer.py --file results/clusters.dat
    python3 tools/viewer.py --file results/clusters.dat --groups 4 --trail 8 \
        --rotate 0.2 --range 130 --save output.gif

--groups colours contiguous particle blocks (the cluster IC appends one block
per cluster), --trail draws the last N frames faintly (save mode only), and
--rotate turns the camera by that many degrees per frame.

The frame format is one "x y z" line per particle, two blank lines between
frames (the format from the assignment). Use --range to freeze the axes; the
default is the extent of the first frame, so escaper particles cannot zoom the
whole view out.
"""

import argparse
import collections
import os
import sys
import tempfile
import threading

import numpy as np

os.environ.setdefault("MPLCONFIGDIR", os.path.join(tempfile.gettempdir(), "nbody-mpl"))
import matplotlib

if "--save" in sys.argv or any(a.startswith("--save=") for a in sys.argv):
    matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


def read_frames(stream):
    frame = []
    for line in stream:
        text = line.strip()
        if not text:
            if frame:
                yield np.asarray(frame, dtype=float)
                frame = []
            continue
        frame.append([float(v) for v in text.split()])
    if frame:
        yield np.asarray(frame, dtype=float)


class LatestFrame:
    """Holds only the newest frame; the reader never blocks the simulation."""

    def __init__(self):
        self._mutex = threading.Lock()
        self._frame = None
        self._frames_read = 0
        self.finished = False

    def publish(self, frame):
        with self._mutex:
            self._frame = frame
            self._frames_read += 1

    def take(self):
        with self._mutex:
            frame, self._frame = self._frame, None
            return frame

    @property
    def count(self):
        with self._mutex:
            return self._frames_read


def make_plot(point_size):
    figure = plt.figure(figsize=(7, 7))
    axes = figure.add_subplot(projection="3d")
    axes.set_box_aspect((1, 1, 1))
    axes.set_xlabel("x")
    axes.set_ylabel("y")
    axes.set_zlabel("z")
    scatter = axes.scatter([], [], [], s=point_size, depthshade=False)
    plt.tight_layout()
    return figure, axes, scatter


def decimate(points, max_points):
    stride = max(1, len(points) // max_points)
    return points[::stride]


def set_limits(axes, points, fixed_range):
    if fixed_range is not None:
        limit = fixed_range
    else:
        extent = float(np.abs(points).max()) if len(points) else 1.0
        limit = 1.1 * max(extent, 1e-6)
    axes.set_xlim(-limit, limit)
    axes.set_ylim(-limit, limit)
    axes.set_zlim(-limit, limit)


PALETTE = ["#4c72b0", "#dd8452", "#55a868", "#c44e52", "#8172b3", "#937860"]


def group_colours(n, groups):
    if groups <= 1:
        return None
    sizes = [n // groups + (1 if g < n % groups else 0) for g in range(groups)]
    return np.repeat([PALETTE[g % len(PALETTE)] for g in range(groups)], sizes)[:n]


def show_points(scatter, points):
    # No public setter exists for the offsets of a 3D scatter collection.
    scatter._offsets3d = (points[:, 0], points[:, 1], points[:, 2])
    scatter.stale = True


def run_live(frames, max_points, fps, point_size, fixed_range, groups=1):
    figure, axes, scatter = make_plot(point_size)
    colours = {"value": None}
    latest = LatestFrame()
    limits = {"set": False}

    def reader():
        for frame in frames:
            latest.publish(frame)
        latest.finished = True

    threading.Thread(target=reader, daemon=True).start()

    def update(_):
        points = latest.take()
        if points is not None:
            shown = decimate(points, max_points)
            show_points(scatter, shown)
            if colours["value"] is None and groups > 1:
                colours["value"] = group_colours(len(points), groups)
                scatter.set_facecolor(colours["value"])
            if not limits["set"]:
                set_limits(axes, points, fixed_range)
                limits["set"] = True
            state = " (stream ended)" if latest.finished else ""
            figure.suptitle(f"frame {latest.count}{state}")
        return ()

    # Keep a reference: a discarded FuncAnimation may be garbage collected and
    # the window then never updates.
    animation = FuncAnimation(figure, update, interval=1000.0 / fps, blit=False,
                              cache_frame_data=False)
    plt.show()


def run_save(frames, path, max_points, fps, point_size, fixed_range, groups, trail, rotate):
    figure, axes, scatter = make_plot(point_size)
    try:
        first = next(frames)
    except StopIteration:
        sys.exit("error: no frames")
    set_limits(axes, first, fixed_range)
    if groups > 1:
        scatter.set_facecolor(group_colours(len(first), groups))
    trail_scatter = None
    trail_colours = None
    trail_stride = 16
    if trail:
        trail_scatter = axes.scatter([], [], [], s=0.5 * point_size, depthshade=False,
                                     alpha=0.3)
        colours = group_colours(len(first), groups)
        if colours is not None:
            trail_colours = colours[::trail_stride]
    history = collections.deque(maxlen=trail)
    state = {"frame": 0, "azim": axes.azim}

    def update(points):
        show_points(scatter, decimate(points, max_points))
        if trail_scatter is not None:
            history.append(points[::trail_stride])
            history_points = np.concatenate(history)
            show_points(trail_scatter, history_points)
            if trail_colours is not None:
                trail_scatter.set_facecolor(np.tile(trail_colours, len(history)))
        if rotate:
            state["frame"] += 1
            axes.view_init(elev=axes.elev, azim=state["azim"] + rotate * state["frame"])
        return ()

    # Materialise the frames: FuncAnimation drops the last frame of a bare
    # generator (and needs a length for the gif frame count anyway).
    frames = [first, *frames]
    animation = FuncAnimation(figure, update, frames=frames, interval=1000.0 / fps,
                              blit=False, cache_frame_data=False, repeat=False)
    animation.save(path, writer="pillow", fps=fps, dpi=100)
    print(f"wrote {path}", file=sys.stderr)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--file", help="read frames from this file instead of stdin")
    parser.add_argument("--save", help="save an animated gif here instead of opening a window")
    parser.add_argument("--range", type=float,
                        help="fix every axis to [-RANGE, RANGE] (default: first frame)")
    parser.add_argument("--fps", type=float, default=30.0)
    parser.add_argument("--max-points", type=int, default=4000,
                        help="subsample larger frames for speed")
    parser.add_argument("--point-size", type=float, default=2.0)
    parser.add_argument("--groups", type=int, default=1,
                        help="colour particles by N contiguous index blocks (cluster IC)")
    parser.add_argument("--trail", type=int, default=0,
                        help="draw the previous N frames as faint trails (--save only)")
    parser.add_argument("--rotate", type=float, default=0.0,
                        help="rotate the camera by this many degrees per frame")
    return parser.parse_args()


def main():
    args = parse_args()
    if not args.save:
        backend = matplotlib.get_backend().lower()
        if backend in {"agg", "pdf", "ps", "svg", "cairo", "template"}:
            sys.exit(f"error: matplotlib backend '{backend}' cannot open a window; "
                     "use --save, or forward X11 (ssh -X)")

    stream = open(args.file) if args.file else sys.stdin
    try:
        frames = read_frames(stream)
        if args.save:
            run_save(frames, args.save, args.max_points, args.fps, args.point_size, args.range,
                     args.groups, args.trail, args.rotate)
        else:
            run_live(frames, args.max_points, args.fps, args.point_size, args.range, args.groups)
    except KeyboardInterrupt:
        pass
    finally:
        if args.file:
            stream.close()


if __name__ == "__main__":
    main()
