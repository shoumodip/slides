#!/bin/sh

[ -d thirdparty/raylib ] || ./thirdparty/raylib.sh

echo "[INFO] Generating font header"
xxd -a -i -n font fonts/DMSans-Regular.ttf > fonts/DMSans-Regular.c

echo "[INFO] Building slides"
cc -std=c11 -Ithirdparty/raylib/include -o slides main.c -Lthirdparty/raylib/lib -l:libraylib.a -lm
