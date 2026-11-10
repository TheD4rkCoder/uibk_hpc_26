#!/usr/bin/env gnuplot
# Renders frames from nbody (one "x y z" line per particle, blank line between
# frames) into an animated gif.
# Usage: gnuplot tools/animate.plt
#   gnuplot -e "input='run.dat'; output='run.gif'; frames=300" tools/animate.plt
# range > 0 fixes every axis to [-range:range]; otherwise the first frame sets
# the axes. A fixed range is usually nicer: a few escapers would otherwise
# shrink the visible cluster to a dot.

if (!exists("input")) input = "data.dat"
if (!exists("output")) output = "output.gif"
if (!exists("frames")) frames = 100
if (!exists("delay")) delay = 5
if (!exists("range")) range = 0
if (!exists("pointsize")) pointsize = 0.25

set view 60, 30
set ticslevel 0
if (range > 0) {
    xmin = -range; xmax = range
    ymin = -range; ymax = range
    zmin = -range; zmax = range
} else {
    set terminal unknown
    splot input index 0 using 1:2:3 with points pointtype 7 pointsize pointsize notitle
    xmin = GPVAL_X_MIN; xmax = GPVAL_X_MAX
    ymin = GPVAL_Y_MIN; ymax = GPVAL_Y_MAX
    zmin = GPVAL_Z_MIN; zmax = GPVAL_Z_MAX
}

set terminal gif animate delay delay size 640,640
set output output
set view 60, 30
set ticslevel 0
set border 4095
set xlabel "x"
set ylabel "y"
set zlabel "z"
set xrange [xmin:xmax]
set yrange [ymin:ymax]
set zrange [zmin:zmax]

do for [i=0:frames-1] {
    splot input index i using 1:2:3 with points pointtype 7 pointsize pointsize notitle
}
unset output
print "wrote ".output
