INCLUDEPATH -= $$ROOT_DIR/common/src

win* {
    DEFINES += NX_NETWORK_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}
