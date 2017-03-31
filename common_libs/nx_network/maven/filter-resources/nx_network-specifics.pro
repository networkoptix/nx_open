INCLUDEPATH += ${root.dir}/common_libs/udt/src
INCLUDEPATH += ${root.dir}/common_libs/nx_network/src/nx/network/websocket/

win* {
    DEFINES += NX_NETWORK_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS += -Werror
}
