"""Build every generated figure, for every deck in this project.

    python tools/build_figs.py                 # build what is out of date
    python tools/build_figs.py --force         # rebuild everything
    python tools/build_figs.py 02_mpi          # only decks matching a substring
    python tools/build_figs.py s33             # only figures matching a substring

A "deck" is any top-level directory that contains a figs/ subdirectory.
For each deck this script:

  1. compiles every  <deck>/figs/*.tex  to  <deck>/figs/out/*.svg
     (latex -> dvi -> dvisvgm). A file whose name starts with "_" is treated
     as an include shared by several figures of that deck, not as a figure in
     its own right -- it is never compiled, but changing it rebuilds the deck.
  2. runs every  <deck>/figs/scripts/make_*.py  (matplotlib charts, generated
     tables, ... -- anything deck-specific that produces a figure from data).

Every figure exists in two variants, because a deck can be viewed with either
theme and its PDF is always printed light:

    <deck>/figs/out/<name>.svg          dark  -- black slide
    <deck>/figs/out/light/<name>.svg    light -- white slide, and the PDF

Both come out of the same source: for the light pass this script prepends
\\def\\figlight{} to the LaTeX run, which flips the `Theme roles` block of
shared/figs/preamble.tex, and the generators do the same through
shared/figs/figtheme.py.

The shared TikZ preamble lives in shared/figs/preamble.tex and is put on
TEXINPUTS, so every figure source can simply say \\input{preamble.tex}
regardless of which deck it belongs to. shared/figs is on PYTHONPATH for the
same reason, so a generator can simply say `from figtheme import themes`.

Quarto runs this automatically via the `pre-render` hook in _quarto.yml.
"""
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHARED_FIGS = os.path.join(ROOT, "shared", "figs")
PREAMBLE = os.path.join(SHARED_FIGS, "preamble.tex")
NOT_A_DECK = {"shared", "tools", ".git", ".quarto"}


def decks():
    for name in sorted(os.listdir(ROOT)):
        if name in NOT_A_DECK or name.startswith("."):
            continue
        if os.path.isdir(os.path.join(ROOT, name, "figs")):
            yield name


def newer(src, dst):
    return not os.path.exists(dst) or os.path.getmtime(src) > os.path.getmtime(dst)


def run(cmd, cwd, env=None):
    return subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors="replace")


def tex_env():
    """Put the shared preamble on TeX's search path (trailing separator keeps
    the default paths)."""
    env = dict(os.environ)
    env["TEXINPUTS"] = SHARED_FIGS + os.pathsep + env.get("TEXINPUTS", "")
    return env


def script_env():
    """Let a deck's generators import shared/figs/figtheme.py."""
    env = dict(os.environ)
    env["PYTHONPATH"] = SHARED_FIGS + os.pathsep + env.get("PYTHONPATH", "")
    return env


def includes(figs_dir):
    """Deck-local _*.tex files shared by several figures of that deck."""
    return [os.path.join(figs_dir, f) for f in os.listdir(figs_dir)
            if f.startswith("_") and f.endswith(".tex")]


def out_dirs(figs_dir):
    """Where each theme's figures go: dark in out/, light in out/light/."""
    out = os.path.join(figs_dir, "out")
    return [("dark", out), ("light", os.path.join(out, "light"))]


def build_tikz(figs_dir, tex, force):
    name = os.path.splitext(os.path.basename(tex))[0]
    tmp = os.path.join(figs_dir, ".build")
    deps = [tex, PREAMBLE] + includes(figs_dir)
    todo = [(theme, d, os.path.join(d, name + ".svg"))
            for theme, d in out_dirs(figs_dir)]
    todo = [t for t in todo if force or any(newer(d, t[2]) for d in deps)]
    if not todo:
        return None
    os.makedirs(tmp, exist_ok=True)
    t0 = time.time()

    for theme, out_dir, svg in todo:
        os.makedirs(out_dir, exist_ok=True)
        # The light pass flips the theme roles of the shared preamble. Passing
        # LaTeX code instead of a file name would make the job name "texput",
        # so it is set explicitly; the code itself uses the bare file name,
        # which resolves because latex runs with figs_dir as its directory.
        light = "\\def\\figlight{}\\input{%s}" % os.path.basename(tex)
        arg = tex if theme == "dark" else light
        r = run(["latex", "-interaction=nonstopmode", "-halt-on-error",
                 "-jobname=" + name, "-output-directory=" + tmp, arg],
                figs_dir, tex_env())
        if r.returncode != 0:
            errs = [l for l in r.stdout.splitlines() if l.startswith("!")]
            return (name, "LaTeX failed (%s): " % theme +
                    ("; ".join(errs[:2]) or "see log"), r.stdout)

        r = run(["dvisvgm", "--no-fonts", "--exact-bbox", "-o", svg,
                 os.path.join(tmp, name + ".dvi")], figs_dir)
        if r.returncode != 0:
            return (name, "dvisvgm failed (%s)" % theme, r.stdout)
    return (name, None, "%.1fs" % (time.time() - t0))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    force = "--force" in sys.argv

    built = skipped = 0
    failed = []
    for deck in decks():
        if args and not any(a in deck for a in args):
            # a filter may name a deck *or* a figure; only skip the deck when
            # no figure inside it matches either
            figs = os.path.join(ROOT, deck, "figs")
            names = [f for f in os.listdir(figs)
                     if f.endswith(".tex") and not f.startswith("_")]
            if not any(a in n for a in args for n in names):
                continue

        figs_dir = os.path.join(ROOT, deck, "figs")
        texs = sorted(f for f in os.listdir(figs_dir)
                      if f.endswith(".tex") and not f.startswith("_"))
        if args:
            texs = [t for t in texs
                    if any(a in t for a in args) or any(a in deck for a in args)]

        header_done = False
        for tex in texs:
            res = build_tikz(figs_dir, os.path.join(figs_dir, tex), force)
            if res is None:
                skipped += 1
                continue
            if not header_done:
                print("[%s]" % deck)
                header_done = True
            name, err, info = res
            if err:
                failed.append((deck, name, err, info))
                print("  FAIL  %-20s %s" % (name, err))
            else:
                built += 1
                print("  ok    %-20s %s" % (name, info))

        # deck-specific generators (charts, tables, ...)
        scripts_dir = os.path.join(figs_dir, "scripts")
        if os.path.isdir(scripts_dir) and not any(a in " ".join(texs) for a in args if args):
            for script in sorted(f for f in os.listdir(scripts_dir)
                                 if f.startswith("make_") and f.endswith(".py")):
                if not header_done:
                    print("[%s]" % deck)
                    header_done = True
                r = run([sys.executable, os.path.join(scripts_dir, script)],
                        figs_dir, script_env())
                ok = r.returncode == 0
                print("  %-5s %s" % ("ok" if ok else "FAIL", script))
                if not ok:
                    print(r.stdout[-1200:])
                    failed.append((deck, script, "script failed", ""))

    print("\n%d built, %d up to date, %d failed" % (built, skipped, len(failed)))
    if failed:
        for deck, name, err, log in failed:
            if log and len(log) > 60:
                print("\n----- %s / %s -----\n%s" % (deck, name, log[-1600:]))
        sys.exit(1)

    for deck in decks():
        shutil.rmtree(os.path.join(ROOT, deck, "figs", ".build"), ignore_errors=True)


if __name__ == "__main__":
    main()
