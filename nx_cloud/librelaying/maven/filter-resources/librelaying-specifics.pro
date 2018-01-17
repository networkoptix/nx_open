INCLUDEPATH -= $$ROOT_DIR/common/src

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}

win* {
    DEFINES -= NX_RELAYING_API=__declspec(dllimport)
    DEFINES += NX_RELAYING_API=__declspec(dllexport)
}
