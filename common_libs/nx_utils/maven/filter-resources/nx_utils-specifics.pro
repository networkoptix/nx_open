QT -= gui

win* {
    DEFINES += NX_UTILS_API=__declspec(dllexport)
}

win* {
    PRECOMPILED_HEADER = StdAfx.h
    PRECOMPILED_SOURCE = StdAfx.cpp
}    

SOURCES += ${project.build.directory}/nx_utils_app_info_impl.cpp
