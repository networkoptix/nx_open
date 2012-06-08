set -e

if [ -z "$1" ]
then
    CONFIG=debug
else
    CONFIG=$1
fi

[ "$CONFIG" != "debug" -a "$CONFIG" != "release" ] && (echo "Usage: build.sh <debug|release>"; exit -1)

case `uname -s` in
    "Linux")
        PLATFORM=linux
        NPROCESSORS=$[$(cat /proc/cpuinfo | grep ^processor | wc -l)]
        PRODUCTS="common mediaserver mediaproxy"
        ;;
    "Darwin")
        PLATFORM=mac
        NPROCESSORS=`sysctl hw.ncpu | awk '{print $2}'`
        FFMPEG_VERSION=`grep ^FFMPEG_VERSION common/convert.py | sed "s/FFMPEG_VERSION = \'\(.*\)\'/\1/g"`
        PRODUCTS="common mediaserver mediaproxy client"
        export DYLD_LIBRARY_PATH=$PWD/../ffmpeg/ffmpeg-git-$FFMPEG_VERSION-mac-release/lib:$DYLD_LIBRARY_PATH
        ;;
esac

for i in $PRODUCTS appserver
do
  pushd $i
  python convert.py
  popd
done

./fix_makefiles.sh

rm mediaserver/build/Makefile.debug.bak mediaserver/build/Makefile.release.bak client/build/Makefile.debug.bak client/build/Makefile.release.bak

for i in $PRODUCTS
do
  pushd $i/build
  make -f Makefile.$CONFIG -j $[NPROCESSORS+1]
  popd
done
