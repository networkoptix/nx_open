TEMPLATE = app
CONFIG += console

DEFINES+=BOOST_BIND_NO_PLACEHOLDERS

INCLUDEPATH += ${root.dir}/appserver2/src
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/nx_cloud/nx_data_sync_engine/src/
INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src/
INCLUDEPATH += ${root.dir}/mediaserver_db/src/

linux {
    QMAKE_CXXFLAGS += -Werror -Wno-error=strict-aliasing
}
