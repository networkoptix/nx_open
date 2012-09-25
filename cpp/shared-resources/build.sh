#!/bin/bash
export buildlib=${buildLib}

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

make -f Makefile.${build.configuration} -j $[NPROCESSORS+1] ;; exit 1

if [[ $buildlib != 'staticlib' ]]; then
  echo "export LD_LIBRARY_PATH=${libdir}/build/bin/${build.configuration}:/usr/lib" > ${libdir}/bin/${build.configuration}/env.sh
  mv ${libdir}/bin/${build.configuration}/${project.artifactId} ${libdir}/bin/${build.configuration}/${project.artifactId}-bin
fi