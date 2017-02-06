QT -= gui

win* {
    DEFINES += NX_UTILS_API=__declspec(dllexport)
}

win* {
    PRECOMPILED_HEADER = StdAfx.h
    PRECOMPILED_SOURCE = StdAfx.cpp
}    

SOURCES += ${project.build.directory}/app_info_impl.cpp

ios: OBJECTIVE_SOURCES += ${basedir}/src/nx/utils/log/log_ios.mm
