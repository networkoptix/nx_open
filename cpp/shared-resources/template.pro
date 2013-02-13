NAME=${project.artifactId}
BUILDLIB = ${buildLib}
TARGET = ${project.artifactId}
VERSION = ${project.version}
QMAKE_INFO_PLIST = Info.plist
CONFIG += precompile_header $$BUILDLIB
CONFIG -= flat app_bundle
DEFINES += __STDC_CONSTANT_MACROS
RESOURCES += ${project.build.directory}/build/${project.artifactId}-common.qrc
RESOURCES += ${project.build.directory}/build/${project.artifactId}-custom.qrc
RESOURCES += ${project.build.directory}/build/${project.artifactId}.qrc
RESOURCES += ${project.build.directory}/build/${project.artifactId}-generated.qrc
RESOURCES += ${project.build.directory}/build/${project.artifactId}-translations.qrc

isEmpty(BUILDLIB) {
  ICON = ${project.build.directory}/hdw_logo.ico
}

CONFIG(debug, debug|release) {
  isEmpty(BUILDLIB) {
	DESTDIR = ../../build-environment/${arch}/bin/debug
#	PRE_TARGETDEPS += ${libdir}/build/bin/debug/common.lib
	} else {
    DESTDIR = ../../build-environment/${arch}/build/bin/debug
  }  
  OBJECTS_DIR = ../${arch}/build/debug
  MOC_DIR = ../${arch}/build/debug/generated
  UI_DIR = ../${arch}/build/debug/generated
  RCC_DIR = ../${arch}/build/debug/generated
  LIBS = -L${libdir}/build/bin/debug -L${environment.dir}/qt/bin/${arch}/debug
}

CONFIG(release, debug|release) {
  isEmpty(BUILDLIB) {
	DESTDIR = ../../build-environment/${arch}/bin/release
#	PRE_TARGETDEPS += ${libdir}/build/bin/debug/common.lib
  } else {
    DESTDIR = ../../build-environment/${arch}/build/bin/release
  }  
  OBJECTS_DIR  = ../${arch}/build/release
  MOC_DIR = ../${arch}/build/release/generated
  UI_DIR = ../${arch}/build/release/generated
  RCC_DIR = ../${arch}/build/release/generated
  LIBS = -L${libdir}/build/bin/release -L${environment.dir}/qt/bin/${arch}/release
}

!contains(TARGET,common) {
  LIBS += -lcommon	
}

LIBS += ${global.libs}
DEFINES += ${global.defines}

QT += ${qtlib1} ${qtlib2} ${qtlib3} ${qtlib4} ${qtlib5} ${qtlib6} ${qtlib7} ${qtlib8} ${qtlib9}

include(${environment.dir}/qt/custom/QtCore/private/qtcore.pri)
INCLUDEPATH += ${project.build.sourceDirectory} ${project.build.directory}  ${basedir}/../common/src ${libdir}/build/include ${project.build.directory}/build/include ${environment.dir}/qt/custom ${environment.dir}/qt/custom/QtCore
DEPENDPATH *= $${INCLUDEPATH}

PRECOMPILED_HEADER = ${project.build.sourceDirectory}/StdAfx.h
PRECOMPILED_SOURCE = ${project.build.sourceDirectory}/StdAfx.cpp

win* {
  isEmpty(BUILDLIB) {
    RC_FILE = ${project.build.directory}/hdwitness.rc
	ICON = ${project.build.directory}/hdw_logo.ico	
  }
  
  CONFIG += ${arch}
  LIBS += ${windows.oslibs}
  DEFINES += ${windows.defines}  
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
  }
  
  !staticlib {
    DEFINES += QN_EXPORT=Q_DECL_EXPORT
  }

  staticlib {
    DEFINES += QN_EXPORT=
  }
  
  QMAKE_MOC = $$QMAKE_MOC -DQ_OS_WIN
}

unix {
  LIBS += ${linux.oslibs}
  DEFINES += QN_EXPORT=
  QMAKE_CXXFLAGS += -msse2 -std=c++0x -fpermissive
  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas
  DEFINES += ${linux.defines}
  QMAKE_MOC = $$QMAKE_MOC -DQ_OS_LINUX
  DEFINES += override=
}

mac {
  DEFINES += QN_EXPORT=  
  LIBS += ${mac.oslibs}
  DEFINES += ${mac.defines}
}




