INCLUDEPATH += ${root.dir}/appserver2/src/

win* {
    DEFINES += NX_DATA_SYNC_ENGINE_API=__declspec(dllexport)
}

# boost-1.60 produces strict aliasing warnings when compiled with gcc 7.1. Probably, boost upgrade will help.
linux {
    QMAKE_CXXFLAGS += -Werror -Wno-error=strict-aliasing
    QMAKE_LFLAGS += -Wl,--no-undefined
}
