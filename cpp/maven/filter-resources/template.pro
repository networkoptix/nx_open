NAME=${project.artifactId}
BUILDLIB = ${buildLib}
TARGET = ${project.artifactId}
VERSION = ${release.version}
QT += ${qt.libs}

## GLOBAL CONFIGURATIONS
QMAKE_INFO_PLIST = Info.plist
CONFIG += precompile_header $$BUILDLIB
CONFIG -= flat
DEFINES += USE_NX_HTTP __STDC_CONSTANT_MACROS ${global.defines}
RESOURCES += ${project.build.directory}/build/${project.artifactId}.qrc

CONFIG(debug, debug|release) {
  CONFIGURATION=debug
  isEmpty(BUILDLIB) {
    CONFIG += console
  }
  win* {
    LIBS = ${windows.oslibs.debug}
  }
}
else {
  CONFIG += silent
  CONFIGURATION=release
  win* {
    LIBS = ${windows.oslibs.release}
  }
}

isEmpty(BUILDLIB) {
DESTDIR = ${libdir}/${arch}/bin/$$CONFIGURATION
} else {
  DESTDIR = ${libdir}/${arch}/lib/$$CONFIGURATION
}  

OBJECTS_DIR = ${project.build.directory}/build/$$CONFIGURATION
MOC_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
UI_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
RCC_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
LIBS += -L${libdir}/${arch}/lib/$$CONFIGURATION -L${qt.dir}/lib
LIBS += ${global.libs}

INCLUDEPATH +=  ${qt.dir}/include \
                ${qt.dir}/include/QtCore \
                ${project.build.sourceDirectory} \
                ${project.build.directory} \
                ${root.dir}/common/src \
                ${libdir}/include \
                ${qt.dir}/../qt-custom \
                ${qt.dir}/include/QtCore/$$QT_VERSION/ \
                ${qt.dir}/include/QtCore/$$QT_VERSION/QtCore/
                ${qt.dir}/include/QtWidgets/$$QT_VERSION/QtWidgets/ \

DEPENDPATH *= $${INCLUDEPATH}

!mac {
  PRECOMPILED_HEADER = ${project.build.sourceDirectory}/StdAfx.h
  PRECOMPILED_SOURCE = ${project.build.sourceDirectory}/StdAfx.cpp
}

win* {
  RC_FILE = ${project.build.directory}/hdwitness.rc
  ICON = ${libdir}/icons/hdw_logo.ico	
  CONFIG += ${arch}
  LIBS += ${windows.oslibs}
  DEFINES += ${windows.defines}  
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
	# /OPT:NOREF is here for a reason, see http://stackoverflow.com/questions/6363991/visual-studio-debug-information-in-release-build.
	QMAKE_CXXFLAGS_RELEASE += /Zi /wd4250
	QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:NOREF 
    QMAKE_LFLAGS += /MACHINE:${arch} /LARGEADDRESSAWARE
  }
  
  !staticlib {
    DEFINES += QN_EXPORT=Q_DECL_EXPORT
  }
  else {
    DEFINES += QN_EXPORT=
  }
  
  QMAKE_MOC = $$QMAKE_MOC -DQ_OS_WIN
}

## BOTH LINUX AND MAC
unix: {
  DEFINES += override=
  DEFINES += QN_EXPORT=  
}

## LINUX
unix:!mac {
  LIBS += ${linux.oslibs}
  QMAKE_CXXFLAGS += -msse2 -std=c++0x -fpermissive
  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas -Wno-ignored-qualifiers
  DEFINES += ${linux.defines}
  QMAKE_MOC = $$QMAKE_MOC -DQ_OS_LINUX
}

## MAC OS
mac {
  QMAKE_CXXFLAGS += -msse4.1 -std=c++0x -fpermissive
  QMAKE_CFLAGS += -msse4.1
  LIBS += ${mac.oslibs}
  DEFINES += ${mac.defines}
  CONFIG -= app_bundle objective_c
  INCLUDEPATH +=  ${environment.dir}/include/glext/
  QMAKE_CFLAGS_PPC_64     -= -arch ppc64 -Xarch_ppc64 -mmacosx-version-min=10.5
  QMAKE_OBJECTIVE_CFLAGS_PPC_64  -= -arch ppc64 -Xarch_ppc64 -mmacosx-version-min=10.5
  QMAKE_CFLAGS_X86_64     -= -arch x86_64 -Xarch_x86_64 -mmacosx-version-min=10.5
  QMAKE_OBJECTIVE_CFLAGS_X86_64  -= -arch x86_64 -Xarch_x86_64 -mmacosx-version-min=10.5
  QMAKE_CXXFLAGS_PPC_64   -= -arch ppc64 -Xarch_ppc64 -mmacosx-version-min=10.5
  QMAKE_CXXFLAGS_X86_64   -= -arch x86_64 -Xarch_x86_64 -mmacosx-version-min=10.5
  QMAKE_LFLAGS_PPC_64     -= -arch ppc64 -Xarch_ppc64 -mmacosx-version-min=10.5
  QMAKE_LFLAGS_X86_64     -= -arch x86_64 -Xarch_x86_64 -mmacosx-version-min=10.5
}