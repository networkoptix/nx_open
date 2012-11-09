#!/bin/bash
export buildlib=${buildLib}

CONFIG=${build.configuration}
ARTIFACT=${project.artifactId}

case `uname -s` in
    "Linux")
        PLATFORM=linux
        NPROCESSORS=$[$(cat /proc/cpuinfo | grep ^processor | wc -l)]
        PRODUCTS="common mediaserver mediaproxy client"
        ;;
    "Darwin")
        PLATFORM=mac
        NPROCESSORS=`sysctl hw.ncpu | awk '{print $2}'`
        FFMPEG_VERSION=`grep ^FFMPEG_VERSION common/convert.py | sed "s/FFMPEG_VERSION = \'\(.*\)\'/\1/g"`
        PRODUCTS="common mediaserver mediaproxy client"
        export DYLD_LIBRARY_PATH=$PWD/../ffmpeg/ffmpeg-git-$FFMPEG_VERSION-mac-release/lib:$DYLD_LIBRARY_PATH
        ;;
esac

make -f Makefile.$CONFIG -j $[NPROCESSORS+1] || exit 1

if [[ $buildlib != 'staticlib' ]]; then
  echo "export LD_LIBRARY_PATH=${libdir}/build/bin/$CONFIG:/usr/lib" > ${libdir}/bin/$CONFIG/env.sh
  mv ${libdir}/bin/$CONFIG/$ARTIFACT ${libdir}/bin/$CONFIG/$ARTIFACT-bin
fi
