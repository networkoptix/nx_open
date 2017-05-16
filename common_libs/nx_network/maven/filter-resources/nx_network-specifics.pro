INCLUDEPATH += ${root.dir}/common_libs/udt/src

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_streaming/src

win* {
    DEFINES += NX_NETWORK_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS += -Werror
}

SOURCES += ${project.build.directory}/app_info_impl.cpp
