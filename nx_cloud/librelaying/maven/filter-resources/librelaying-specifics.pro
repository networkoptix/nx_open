INCLUDEPATH -= $$ROOT_DIR/common/src

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

SOURCES += ${project.build.directory}/librelaying_app_info_impl.cpp

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}

win* {
    DEFINES += __declspec(dllexport)
}
