INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/

!android:!mac {
    QT += zlib-private
}

macx: {
  OBJECTIVE_SOURCES += ${basedir}/src/utils/mac_utils.mm
  LIBS += -lobjc -framework Foundation -framework AppKit
}

linux {
    QMAKE_LFLAGS += -Wl,--no-undefined
}

SOURCES += ${project.build.directory}/app_info_impl.cpp
