INCLUDEPATH += ${root.dir}/common_libs/nx_cassandra/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/librelaying/src/

INCLUDEPATH -= $$ROOT_DIR/common/src

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

SOURCES += ${project.build.directory}/libtraffic_relay_app_info_impl.cpp

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}
