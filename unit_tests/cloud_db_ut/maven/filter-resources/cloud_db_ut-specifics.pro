TEMPLATE = app
CONFIG += console

DEFINES+=BOOST_BIND_NO_PLACEHOLDERS

INCLUDEPATH += ${root.dir}/appserver2/src
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/

linux {
    QMAKE_CXXFLAGS += -Werror -Wno-error=strict-aliasing
}
