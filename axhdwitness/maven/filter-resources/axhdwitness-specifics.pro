TEMPLATE = lib
DEF_FILE = ${basedir}/contrib/activeqt/control/qaxserver.def
#RC_FILE  = ${project.build.sourceDirectory}/axhdwitness.rc

DEFINES += HDW_SDK
# CL_TRIAL_MODE 
DEFINES += CL_FORCE_LOGO

INCLUDEPATH +=  ${qt.dir}/include/QtWidgets/$$QT_VERSION/ \
                ${qt.dir}/include/QtWidgets/$$QT_VERSION/QtWidgets/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \
                ${root.dir}/appserver2/src/ \
                ${root.dir}/client/src/

CONFIG(debug, debug|release) {
  QMAKE_POST_LINK = ../post.bat debug ${arch};
}

CONFIG(release, debug|release) {
  QMAKE_POST_LINK = ../post.bat release ${arch};
}

win32 {
  QT += axserver
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

