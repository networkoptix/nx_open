INCLUDEPATH += ${root.dir}/common_libs/udt/src

win* {
    DEFINES += NX_NETWORK_API=__declspec(dllexport)
}

SOURCES += ${project.build.directory}/app_info_impl.cpp
