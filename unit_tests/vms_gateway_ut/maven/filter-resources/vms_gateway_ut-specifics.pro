TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/nx_fusion/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include
INCLUDEPATH += ${root.dir}/nx_cloud/libvms_gateway/src
INCLUDEPATH += ${root.dir}/nx_cloud/

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_streaming/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
