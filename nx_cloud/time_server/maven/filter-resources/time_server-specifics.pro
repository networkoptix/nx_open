TEMPLATE = app
CONFIG += console

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

SOURCES += ${project.build.directory}/time_server_app_info_impl.cpp

linux {
    QMAKE_CXXFLAGS += -Werror
}
