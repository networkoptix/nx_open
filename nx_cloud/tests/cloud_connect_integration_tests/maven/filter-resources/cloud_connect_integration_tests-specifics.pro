TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
