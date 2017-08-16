INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/

linux {
    QMAKE_LFLAGS += -Wl,--no-undefined
}
