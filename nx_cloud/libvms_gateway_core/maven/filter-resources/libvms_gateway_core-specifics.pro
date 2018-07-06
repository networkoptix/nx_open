INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/librelaying/src/

QMAKE_RESOURCE_FLAGS += -name vms_gateway_core

win* {
    DEFINES+=_VARIADIC_MAX=8
}

SOURCES += ${project.build.directory}/libvms_gateway_core_app_info_impl.cpp

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}

win* {
    DEFINES -= NX_VMS_GATEWAY_API=__declspec(dllimport)
    DEFINES += NX_VMS_GATEWAY_API=__declspec(dllexport)
}
