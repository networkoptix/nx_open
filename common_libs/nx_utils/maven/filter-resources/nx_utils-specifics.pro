QT -= gui

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_network/src \
               $$ROOT_DIR/common_libs/nx_streaming/src \
               $$ROOT_DIR/common_libs/nx_media/src \
               $$ROOT_DIR/common_libs/nx_audio/src

win* {
    DEFINES += NX_UTILS_API=__declspec(dllexport)
}

win* {
    PRECOMPILED_HEADER = StdAfx.h
    PRECOMPILED_SOURCE = StdAfx.cpp
}

linux {
    QMAKE_CXXFLAGS += -Werror
}

SOURCES += ${project.build.directory}/app_info_impl.cpp

ios: OBJECTIVE_SOURCES += ${basedir}/src/nx/utils/log/log_ios.mm
