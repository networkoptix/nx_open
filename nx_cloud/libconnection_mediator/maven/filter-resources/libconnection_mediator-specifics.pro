INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include

SOURCES += ${project.build.directory}/libconnection_mediator_app_info_impl.cpp

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}
