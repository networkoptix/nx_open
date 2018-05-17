INCLUDEPATH += $$ROOT_DIR/common/src
INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src

win* {
    DEFINES += NX_UPDATE_API=__declspec(dllexport)
}

linux {
    QMAKE_LFLAGS += -Wl,--no-undefined
}
