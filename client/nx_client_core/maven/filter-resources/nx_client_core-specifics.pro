INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/common_libs/nx_vms_utils/src \
                ${root.dir}/nx_cloud/cloud_db_client/src/include

mac:!ios {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers
}

mac:!ios {
    LIBS += -lobjc -framework Foundation -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}
