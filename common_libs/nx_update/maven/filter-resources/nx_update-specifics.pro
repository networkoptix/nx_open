INCLUDEPATH += $$ROOT_DIR/common/src

win* {
    DEFINES += NX_UPDATE_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS += -Werror -Wno-error=unused-function
    QMAKE_LFLAGS += -Wl,--no-undefined
}
