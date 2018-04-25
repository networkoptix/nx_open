INCLUDEPATH += ${root.dir}/appserver2/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/common_libs/nx_email/src/
INCLUDEPATH += ${root.dir}/common_libs/nx_network/src/
INCLUDEPATH += ${root.dir}/common_libs/nx_speech_synthesizer/src/
INCLUDEPATH += ${root.dir}/common_libs/nx_onvif/src/
INCLUDEPATH += ${root.dir}/common_libs/nx_update/src
INCLUDEPATH += ${root.dir}/nx_system_commands/src
INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src
INCLUDEPATH += ${root.dir}/mediaserver_db/src

SOURCES += ${project.build.directory}/mediaserver_core_app_info_impl.cpp

linux {
    QMAKE_LFLAGS -= -Wl,--no-undefined
}
