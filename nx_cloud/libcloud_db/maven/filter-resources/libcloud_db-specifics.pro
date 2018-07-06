INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/nx_cloud/nx_data_sync_engine/src/
INCLUDEPATH += ${root.dir}/common_libs/nx_email/src/
INCLUDEPATH += ${root.dir}/appserver2/src/
INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src/
INCLUDEPATH += ${root.dir}/mediaserver_db/src/

win* {
    DEFINES+=_VARIADIC_MAX=8
}

SOURCES += ${project.build.directory}/libcloud_db_app_info_impl.cpp

# boost-1.60 produces strict aliasing warnings when compiled with gcc 7.1. Probably, boost upgrade will help.
linux {
    QMAKE_CXXFLAGS += -Werror -Wno-error=strict-aliasing
    QMAKE_LFLAGS += -Wl,--no-undefined
}
