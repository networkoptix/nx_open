TARGET = Ax${ax.className}
DEF_FILE = ${basedir}/contrib/activeqt/control/qaxserver.def

DEFINES += HDW_SDK

INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/client.core/src \
                ${root.dir}/common_libs/nx_vms_utils/src \
                ${root.dir}/common_libs/nx_speach_synthesizer/src/ \
                ${root.dir}/client/src \

LIBS += $$FESTIVAL_LIB

CONFIG(debug, debug|release) {
  QMAKE_POST_LINK = ..\${arch}\post.bat debug ${arch};
}

CONFIG(release, debug|release) {
  QMAKE_POST_LINK = ..\${arch}\post.bat release ${arch};
}

win32 {
  QT += axserver
  DEFINES += AxHDWitness=Ax${ax.className}
  CONFIG += dll qaxserver_no_postlink
  CONFIG -= console
}

HEADERS += ${project.build.directory}/axclient/axclient_app_info.h

win32 {
    PRECOMPILED_HEADER = StdAfx.h
    PRECOMPILED_SOURCE = StdAfx.cpp
}