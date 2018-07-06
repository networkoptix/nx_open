QT -= gui

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_network/src \
               $$ROOT_DIR/common_libs/nx_media/src \
               $$ROOT_DIR/common_libs/nx_audio/src \
               $$ROOT_DIR/common_libs/nx_fusion/src

win* {
    DEFINES += NX_SDK_API=__declspec(dllexport)
}

win* {
    PRECOMPILED_HEADER = StdAfx.h
    PRECOMPILED_SOURCE = StdAfx.cpp
}

linux {
    QMAKE_LFLAGS += -Wl,--no-undefined
}
