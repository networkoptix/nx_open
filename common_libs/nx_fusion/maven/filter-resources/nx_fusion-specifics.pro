win* {
    DEFINES += NX_FUSION_API=__declspec(dllexport)
}

linux {
    QMAKE_LFLAGS += -Wl,--no-undefined
}

INCLUDEPATH -= $$ROOT_DIR/common/src
