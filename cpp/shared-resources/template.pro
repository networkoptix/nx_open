NAME=${project.artifactId}
BUILDLIB = ${buildLib}
TARGET = ${project.artifactId}
VERSION = ${release.version}
QMAKE_INFO_PLIST = Info.plist
CONFIG += precompile_header $$BUILDLIB

CONFIG(release, debug|release) {
  CONFIG += flat silent
}

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
	DESTDIR = ../../build_environment/${arch}/bin/debug
#	PRE_TARGETDEPS += ${libdir}/build/bin/debug/common.lib
	} else {
    DESTDIR = ../../build_environment/${arch}/build/bin/debug
  }  
  OBJECTS_DIR = ../${arch}/build/debug
  MOC_DIR = ../${arch}/build/debug/generated
  UI_DIR = ../${arch}/build/debug/generated
  RCC_DIR = ../${arch}/build/debug/generated
  LIBS = -L${libdir}/build/bin/debug -L${environment.dir}/qt5/qtbase-${arch}/lib
}

CONFIG(release, debug|release) {
  isEmpty(BUILDLIB) {
	DESTDIR = ../../build_environment/${arch}/bin/release
#	PRE_TARGETDEPS += ${libdir}/build/bin/debug/common.lib
  } else {
    DESTDIR = ../../build_environment/${arch}/build/bin/release
  }  
  OBJECTS_DIR  = ../${arch}/build/release
  MOC_DIR = ../${arch}/build/release/generated
  UI_DIR = ../${arch}/build/release/generated
  RCC_DIR = ../${arch}/build/release/generated
  LIBS = -L${libdir}/build/bin/release -L${environment.dir}/qt5/qtbase-${arch}/lib
}

!contains(TARGET,common){
  LIBS += -lcommon	
}

LIBS += ${global.libs}
DEFINES += ${global.defines}

!mac {
    include(${environment.dir}/qt5/qt-custom/QtCore/private/qtcore.pri)
}
INCLUDEPATH += ${environment.dir}/qt5/qtbase-${arch}/include \
		${environment.dir}/qt5/qtbase-${arch}/include/QtCore \
		${project.build.sourceDirectory} \
		${project.build.directory} \
		${basedir}/../common/src ${libdir}/build/include \
		${project.build.directory}/build/include \
		${environment.dir}/qt5/qt-custom \
		${environment.dir}/qt5/qt-custom/QtCore \
		${environment.dir}/qt5/qtbase-${arch}/include/QtCore/5.1.0/ \
		${environment.dir}/qt5/qtbase-${arch}/include/QtCore/5.1.0/QtCore/
DEPENDPATH *= $${INCLUDEPATH}

!mac {
  PRECOMPILED_HEADER = ${project.build.sourceDirectory}/StdAfx.h
  PRECOMPILED_SOURCE = ${project.build.sourceDirectory}/StdAfx.cpp
}

win* {
  isEmpty(BUILDLIB) {
    RC_FILE = ${project.build.directory}/hdwitness.rc
	ICON = ${project.build.directory}/hdw_logo.ico	
  }
  
  CONFIG += ${arch}
  LIBS += ${windows.oslibs}
  DEFINES += _USING_V110_SDK71_ ${windows.defines}  
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
	# /OPT:NOREF is here for a reason, see http://stackoverflow.com/questions/6363991/visual-studio-debug-information-in-release-build.
	QMAKE_CFLAGS_RELEASE += /Zi
	QMAKE_CXXFLAGS_RELEASE += /Zi
	QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:NOREF
  }
  
  !staticlib {
    DEFINES += QN_EXPORT=Q_DECL_EXPORT
    LIBS += DbgHelp.lib
  }

  staticlib {
    DEFINES += QN_EXPORT=
  }
  
  QMAKE_MOC = $$QMAKE_MOC -DQ_OS_WIN
}

unix:!mac {
  LIBS += ${linux.oslibs}
  DEFINES += QN_EXPORT=
  QMAKE_CXXFLAGS += -msse2 -std=c++0x -fpermissive
  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas -Wno-ignored-qualifiers
  DEFINES += ${linux.defines}
  QMAKE_MOC = $$QMAKE_MOC -DQ_OS_LINUX
  DEFINES += override=
}

mac {
  QMAKE_CXXFLAGS += -msse4.1 -std=c++0x -fpermissive
  QMAKE_CFLAGS += -msse4.1
  DEFINES += QN_EXPORT=  
  LIBS += ${mac.oslibs}
  DEFINES += ${mac.defines} override=
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




