set debuginfod enabled off
handle SIGSEGV stop print nopass
run
bt
info registers
x/8i $pc-8
