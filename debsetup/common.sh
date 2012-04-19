VERSION=`python ../../common/common_version.py`
ARCH=`uname -i`

QT_PATH=$(dirname $(dirname $(which qmake)))
case $ARCH in
    "i386")
        QT_LIB_PATH="$QT_PATH/lib/i386-linux-gnu"
        ARCHITECTURE=i386
        ;;
    "x86_64")
        QT_LIB_PATH="$QT_PATH/lib"
        ARCHITECTURE=amd64
        ;;
esac

