INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src/
INCLUDEPATH += ${root.dir}/appserver2/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/

win* {
    DEFINES += NX_VMS_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS -= -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}
