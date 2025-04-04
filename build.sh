#!/bin/sh

[ -d thirdparty/raylib ] || ./thirdparty/raylib.sh

echo "[INFO] Generating font header"
xxd -a -i -n font fonts/DMSans-Regular.ttf > fonts/DMSans-Regular.c

echo "[INFO] Building slides"
cc -Ithirdparty/raylib/include -o slides main.c -lm -lpthread -Lthirdparty/raylib/lib -l:libraylib.a
