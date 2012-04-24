set -e

apt-get install -y libmp3lame-dev libssl-dev libx264-dev libxerces-c-dev libprotobuf-dev libqt4-dev libqjson-dev build-essential python-django python-pip 

pushd appserver
python ./scripts/install_others.py
popd

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
        ;;
    "Darwin")
        PLATFORM=mac
        NPROCESSORS=`sysctl hw.ncpu | awk '{print $2}'`
        FFMPEG_VERSION=`grep ^FFMPEG_VERSION common/convert.py | sed "s/FFMPEG_VERSION = \'\(.*\)\'/\1/g"`
        export DYLD_LIBRARY_PATH=$PWD/../ffmpeg/ffmpeg-git-$FFMPEG_VERSION-mac-release/lib:$DYLD_LIBRARY_PATH
        ;;
esac

for i in common mediaserver client appserver
do
  pushd $i
  python convert.py
  popd
done

# Workaround buggy qtservice
if [ `uname -s` == 'Darwin' ]
then
    SED_ARGS='-i .bak'
else
    # Assume GNU sed
    SED_ARGS='-i.bak'
fi

sed $SED_ARGS "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" mediaserver/build/Makefile.debug
sed $SED_ARGS "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" mediaserver/build/Makefile.debug
sed $SED_ARGS "1,/release\/generated\/qtservice.moc\ \\\\/{/release\/generated\/qtservice.moc\\ \\\\/d;}" mediaserver/build/Makefile.release
sed $SED_ARGS "1,/release\/generated\/qtservice_unix.moc\ \\\\/{/release\/generated\/qtservice_unix.moc\\ \\\\/d;}" mediaserver/build/Makefile.release

sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice.moc\)%\1%" mediaserver/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" mediaserver/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(release\/generated\/qtservice.moc\)%\1%" mediaserver/build/Makefile.release
sed $SED_ARGS "s%^\.\.\/build\/\(release\/generated\/qtservice_unix.moc\)%\1%" mediaserver/build/Makefile.release

sed $SED_ARGS "s%\.\.\/build\/debug%debug%g" client/build/Makefile.debug
sed $SED_ARGS "s%\.\.\/build\/release%release%g" client/build/Makefile.release

rm mediaserver/build/Makefile.debug.bak mediaserver/build/Makefile.release.bak client/build/Makefile.debug.bak client/build/Makefile.release.bak

for i in common mediaserver
do
  pushd $i/build
  make -f Makefile.$CONFIG -j $[NPROCESSORS+1]
  popd
done
