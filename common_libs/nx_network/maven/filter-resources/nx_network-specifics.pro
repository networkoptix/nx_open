INCLUDEPATH += ${root.dir}/common_libs/udt/src

INCLUDEPATH -= $$ROOT_DIR/common/src

win* {
    DEFINES += NX_NETWORK_API=__declspec(dllexport)
}

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}

SOURCES += ${project.build.directory}/app_info_impl.cpp
