TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/nx_data_sync_engine/src
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src/nx/cloud/cdb

linux {
    QMAKE_CXXFLAGS += -Werror
}
