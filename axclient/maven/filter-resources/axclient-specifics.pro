TARGET = Ax${ax.className}
DEF_FILE = ${basedir}/contrib/activeqt/control/qaxserver.def
#RC_FILE  = ${project.build.sourceDirectory}/axhdwitness.rc

DEFINES += HDW_SDK
# CL_TRIAL_MODE 

INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/client.core/src \
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

# IDL_FILES = $$PWD/ItemControl.idl $$PWD/SceneControl.idl 

# idlc.name = Generating code from ${QMAKE_FILE_IN}
# idlc.input = IDL_FILES
# idlc.output = $${MOC_DIR}/${QMAKE_FILE_BASE}.idl.c
# idlc.commands = $${QMAKE_IDL} ${QMAKE_FILE_IN} $${IDLFLAGS} /h $${MOC_DIR}/${QMAKE_FILE_IN}.h /iid $${MOC_DIR}/${QMAKE_FILE_IN}.c
# idlc.CONFIG += target_predeps
# idlc.variable_out = GENERATED_SOURCES
# QMAKE_EXTRA_COMPILERS += idlc

HEADERS += ${project.build.directory}/axclient/axclient_app_info.h

win32 {
    PRECOMPILED_HEADER = StdAfx.h
    PRECOMPILED_SOURCE = StdAfx.cpp
}    