#!/bin/bash
clear
set -x

cd $(dirname "$0")

if g++ -Wfatal-errors -std=c++11 -lEGL -lGLESv2 -o gl_test gl_test.cpp; then
    echo SUCCESS
else
    echo FAILURE
    false
fi
