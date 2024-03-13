#!/bin/sh

set -xe

xxd -a -i -n font fonts/DMSans-Regular.ttf > fonts/DMSans-Regular.c
cc `pkg-config --cflags raylib` -o slides main.c `pkg-config --libs raylib` -lraylib -lm
