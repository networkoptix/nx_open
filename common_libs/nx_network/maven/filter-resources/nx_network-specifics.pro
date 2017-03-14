INCLUDEPATH += ${root.dir}/common_libs/udt/src

win* {
    DEFINES += NX_NETWORK_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS += -Werror
}
