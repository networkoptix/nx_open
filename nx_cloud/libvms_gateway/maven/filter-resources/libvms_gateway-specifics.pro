INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/

win* {
    DEFINES+=_VARIADIC_MAX=8
}

SOURCES += ${project.build.directory}/libvms_gateway_app_info_impl.cpp

linux {
    QMAKE_CXXFLAGS += -Werror
}
