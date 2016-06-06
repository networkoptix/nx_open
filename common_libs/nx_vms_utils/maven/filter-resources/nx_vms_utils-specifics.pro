CONFIG += console

win* {
    DEFINES += NX_VMS_UTILS_API=__declspec(dllexport)
}

SOURCES += ${project.build.directory}/nx_vms_utils_app_info_impl.cpp
