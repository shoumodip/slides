#!/bin/sh

set -xe

FONT="fonts/DMSans-Regular"
[ -f "$FONT.c" ] || xxd -a -i -n font "$FONT.ttf" > "$FONT.c"

cc `pkg-config --cflags raylib` -o slides main.c `pkg-config --libs raylib` -lm -lpthread -lraylib
