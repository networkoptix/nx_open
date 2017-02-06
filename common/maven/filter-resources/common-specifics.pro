!android:!mac {
    QT += zlib-private
}

macx: {
  OBJECTIVE_SOURCES += ${basedir}/src/utils/mac_utils.mm
  LIBS += -lobjc -framework Foundation -framework AppKit
}

SOURCES += ${project.build.directory}/app_info_impl.cpp
