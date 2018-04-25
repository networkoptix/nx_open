INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src/

linux {
    QMAKE_LFLAGS += -Wl,--no-undefined
}
