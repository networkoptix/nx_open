#!/bin/sh

[ -f "$1" ] && echo `grep UUID -A1 -a "$1" | grep -o '[-A-Za-z0-9]\{36\}'`
