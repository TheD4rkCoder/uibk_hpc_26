# 703308 VO High-Performance Computing — lecture slides

Quarto / reveal.js decks, converted from the original PowerPoint files.
One directory per lecture under `lecture/`; the decks are independent of each
other. `lecture/index.html` links them all.

    00_crash_course/                  A Crash Course in Clusters and Job Submission (PS)
    01_motivation_and_crash_course/   Motivation & A Crash Course in Parallel Hard- and Software
    02_mpi_basics/                    MPI – Message Passing Interface
    03_debugging/                     Debugging Parallel Programs
    04_measurements/                  Measuring and Reporting Performance
    05_mpi_advanced/                  MPI Derived Datatypes and Virtual Topologies
    06_dwarfs/                        The 13 Dwarfs of HPC
    07_mpi_advanced_2/                MPI Groups, Communicators and One-Sided Communication

## Render

    quarto render                     # every deck
    quarto render lecture/02_mpi_basics       # one deck
    quarto preview lecture/02_mpi_basics/02_mpi_basics.qmd    # live reload while editing

Each deck renders to `lecture/<deck>/<deck>.html` next to its source. In the
rest of this file, `<deck>/` is short for `lecture/<deck>/`.

PDF for students, either

    quarto render <deck>/<deck>.qmd --to revealjs -M embed-resources:true
    # then open the HTML, press E (print view) and print to PDF, or:
    npx decktape reveal "<deck>/<deck>.html?theme=light" <deck>.pdf

The PDF is always light-themed regardless of how the deck is being shown; see
*Themes* below.

## Layout

    _quarto.yml         project config: the reveal.js settings every deck shares,
                        the pre-render hook that rebuilds stale figures, and
                        lib-dir, which points every deck at one shared _libs/
    _libs/              GENERATED, git-ignored: one copy of reveal.js, MathJax,
                        clipboard.js etc. for all decks (see below)
    shared/
      uibk.scss         UIBK corporate-identity dark theme, used by every deck
      uibk-light.scss   the light variant, as an overlay on top of it
      theme.html        the dark/light switch, included into every deck
      figs/
        preamble.tex    shared TikZ colours, styles and macros for all figures
        figtheme.py     the same colours for the matplotlib generators
        mem.tex         memory layouts (lectures 05, 06)
        numa.tex        the four-socket NUMA node (lectures 01, 07)
    lecture/
      index.html        landing page linking every deck
      <deck>/           one per lecture, see below
    tools/
      build_figs.py     builds every deck's figures (see below)
      pptx_dump.py      inspects a .pptx when converting a new lecture
    <deck>/
      <deck>.qmd        the slide source; carries only title/subtitle/footer,
                        everything else is inherited from _quarto.yml
      figs/
        *.tex           one TikZ figure per file
        _*.tex          shared *within* this deck, never compiled on its own
        data/*.csv      measurement data recovered from the original .pptx
        scripts/        deck-specific generators (charts, tables, compositions)
        out/            GENERATED figures -- disposable, safe to delete
          light/        the same figures, drawn for a white slide
      img/              bitmaps that stay bitmaps (photos, screenshots, clip art)
      code/             source files included into the slides via {{< include >}}
      original/         the .pptx (and .pdf) this deck was converted from

Adding a lecture means creating one more `lecture/<deck>/` directory with a
`.qmd` in it, and a link in `lecture/index.html`. `tools/build_figs.py` picks
it up automatically: a "deck" is simply any directory under `lecture/`
containing a `figs/` subdirectory.

### One shared copy of the JS/CSS support files

By default Quarto gives every document its own `<deck>_files/libs/` tree, so
each deck carried an identical ~5 MB copy of reveal.js, MathJax, clipboard.js
and friends -- 38 MB across seven decks, all of it byte-for-byte the same.
`lib-dir: _libs` in `_quarto.yml` makes Quarto emit one copy at the project
root instead, which every deck references as `../../_libs/...`.

It is generated output, so it is git-ignored; `quarto render` recreates it.

## Themes

The decks ship in two themes and the viewer picks: a **T** on the keyboard, or
the small button in the bottom-left corner, flips between them, and the choice
is remembered per browser.

| | slide | figures |
| --- | --- | --- |
| dark (default) | `shared/uibk.scss` — black slide, the deck as presented | `<deck>/figs/out/*.svg` |
| light | `shared/uibk-light.scss` on top of it — white slide | `<deck>/figs/out/light/*.svg` |

**The PDF is always light.** Printing comes out white-on-paper whatever the
deck is currently showing: `shared/uibk-light.scss` applies unconditionally in
`@media print`, and `shared/theme.html` swaps the figures to their light
variants for the duration of the print job. The print view (`E`, i.e.
`?view=print`) switches to the light theme outright, so what is on screen is
what comes out. For decktape, which drives the deck like a normal viewer, ask
for the light theme in the URL:

    npx decktape reveal "<deck>/<deck>.html?theme=light" <deck>.pdf

`?theme=light` and `?theme=dark` override everything else, which is also the
way to send someone a link to one particular variant.

### How the two halves fit together

reveal.js 5 drives its whole appearance from `--r-*` CSS custom properties, so
the light theme is mostly a matter of redefining those under
`html.theme-light`. Quarto compiles one theme per render, which is why
`uibk-light.scss` is an *overlay* in the `theme:` list rather than a second
theme to switch to.

Figures cannot work that way: an `<img>` does not see the page's CSS. So every
generated figure is built twice, from the same source — `tools/build_figs.py`
prepends a `figlight` flag to the LaTeX run, which flips the `Theme roles`
block of `shared/figs/preamble.tex`, and the matplotlib generators loop over
the two palettes in `shared/figs/figtheme.py`. Switching theme rewrites the
image paths between `figs/out/` and `figs/out/light/`.

Building from source rather than remapping colours in the finished SVGs is
what keeps it maintainable: a figure that mixes its own shade (`blu!60`,
`uibkorange!25`, …) gets that shade recomputed from the new base colour
instead of silently keeping the dark one.

Only the roles that sit on the *slide background* flip. Ink on a fill does
not — a navy fill is dark and an orange one is light in both themes — which is
what the `onfill` and `inkdark` roles are for. Code blocks likewise stay dark
in both: monokai on navy reads as a code card on a white slide just as well as
on a black one.

## Figures

Nothing that is a *diagram* is a bitmap. The rule applied throughout:

| Content | How it is produced |
| --- | --- |
| diagram built from shapes | **TikZ** → SVG (`latex` + `dvisvgm`) |
| graph-shaped diagram | **Graphviz**, inline in the `.qmd` as ` ```{dot} ` |
| chart of real measurements | **matplotlib**, from a CSV under `figs/data/` |
| chart of a formula | **matplotlib**, evaluating the formula itself (no CSV) |
| equation | real LaTeX math in the `.qmd`, never a picture of one |
| table of numbers | Markdown table, or generated HTML from a CSV |
| photo, screenshot, clip art | stays a bitmap in `<deck>/img/` |

PowerPoint rasterises any text run that contains inline math, so a "picture"
in a `.pptx` is not necessarily a picture: in lecture 04 eleven of them turned
out to be equations (or whole bullet lists containing one). Check before
treating an image as an image.

That last row matters: lecture 01 is largely photographs, third-party
screenshots and clip art. Those are *not* redrawn — redrawing a photo is not a
thing. What was rebuilt there is the clip-art *arrangements*
(`figs/scripts/make_compositions.py`), because the arrangement carries the
meaning and screenshotting the slide would have baked in a white background.

Build with:

    python tools/build_figs.py            # only what is out of date
    python tools/build_figs.py --force    # everything
    python tools/build_figs.py 02_mpi     # one deck
    python tools/build_figs.py s33        # one figure

`quarto render` does this for you via the `pre-render` hook, so normally you
never call it by hand. Requires a LaTeX install (TeX Live) plus matplotlib and
Pillow; Graphviz is bundled with Quarto and needs nothing extra.

### Dark-theme images

Third-party screenshots and charts assume a white page: on a black slide they
either glare or, for dark line art on a transparent background, vanish. Mark
those with the `.fig-card` class to put them on a light panel:

    ![](img/image13.png){.fig-card fig-align="center" height="430"}

Photographs do **not** need it — they are self-contained rectangles. The card
disappears by itself in the light theme, where the slide is already white.

### Re-theming

All figure colours live in the `Theme roles` block at the top of
`shared/figs/preamble.tex`, and in `shared/figs/figtheme.py` for the
matplotlib generators. Change them there, run
`python tools/build_figs.py --force`, and every figure in every deck is
re-themed at once.

### Scripts that need the original .pptx

`figs/scripts/make_*.py` run on every build, so they may only read files that
are in the repository. Anything that has to parse the original `.pptx` is a
one-off instead, named `extract_*.py`, run by hand with the deck as an
argument, and its output committed:

    02_mpi_basics/figs/scripts/extract_data.py          -> figs/data/*.csv
    01_.../figs/scripts/extract_compositions.py         -> img/s*.png

The `.pptx` files are git-ignored, so a build step that opened one would break
on any checkout that does not happen to have them.

### Why `02_mpi_basics/figs/s51_table.md` is not in `figs/out/`

Quarto expands `{{< include >}}` *before* it runs the `pre-render` hook, so an
include target inside the disposable `figs/out/` cache makes a clean checkout
fail to render. It therefore lives one level up and should be committed, even
though `make_table.py` generates it.

## Converting another lecture

1. Drop the `.pptx` into `<deck>/original/`.
2. `python tools/pptx_dump.py <deck>/original/<file>.pptx` — prints, per slide,
   the text outline and whether the slide is text, a shape diagram, pictures, a
   chart or a table. Add `--shapes` for exact geometry, `-s N` for one slide,
   `--media DIR` to extract the embedded images.
3. Text → Markdown. Shape diagrams → TikZ in `<deck>/figs/`. Charts and tables
   → a CSV plus a generator in `<deck>/figs/scripts/`. Photos → `<deck>/img/`.
4. For reference renders of the original, export the slides from PowerPoint
   (File → Export → PNG); useful to check a redrawn figure against.
