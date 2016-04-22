QMAKE_RPATHDIR = ""
unix:!mac {
     QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

SOURCES += ${project.build.directory}/applauncher_app_info_impl.cpp
