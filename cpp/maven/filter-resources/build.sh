#!/bin/bash

CONFIG=${build.configuration}
ARTIFACT=${project.artifactId}

case `uname -s` in
    "Linux")
        PLATFORM=linux
        NPROCESSORS=$[$(cat /proc/cpuinfo | grep ^processor | wc -l)]
        ;;
    "Darwin")
        PLATFORM=mac
        NPROCESSORS=`sysctl hw.ncpu | awk '{print $2}'`
        ;;
esac

make --no-p QUIET=yes -f Makefile.$CONFIG -j $[NPROCESSORS+1] || exit 1