QMAKE_RPATHDIR = ""
unix:!mac {
     QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsinglecoreapplication.pri)

SOURCES += ${project.build.directory}/applauncher_app_info_impl.cpp
