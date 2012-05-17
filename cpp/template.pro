NAME=${project.artifactId}
BUILDLIB = ${buildLib}
TARGET = ${project.artifactId}
VERSION = ${project.version}
QMAKE_INFO_PLIST = Info.plist
CONFIG += precompile_header $$BUILDLIB
CONFIG -= flat app_bundle
DEFINES += __STDC_CONSTANT_MACROS

LIBS += ${global.libs}
DEFINES += ${global.defines}

QT *= ${qtlib1} ${qtlib2} ${qtlib3} ${qtlib4} ${qtlib5} ${qtlib6} ${qtlib7} ${qtlib8} ${qtlib9}

INCLUDEPATH += ${project.build.sourceDirectory}  ${basedir}/../common/src ${project.build.directory}/build/include
PRECOMPILED_HEADER = ${project.build.sourceDirectory}/StdAfx.h
PRECOMPILED_SOURCE = ${project.build.sourceDirectory}/StdAfx.cpp

# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION

CONFIG(debug, debug|release) {
  DESTDIR = ${project.build.directory}/bin/debug
  OBJECTS_DIR  = ${project.build.directory}/build/debug
  MOC_DIR = ${project.build.directory}/build/debug/generated
  UI_DIR = ${project.build.directory}/build/debug/generated
  RCC_DIR = ${project.build.directory}/build/debug/generated
  LIBS = -L${project.build.directory}/build/bin/debug -L${environment.dir}/qt/bin/${arch}/debug -L${basedir}/../common/${arch}/bin/debug $$LIBS
}

CONFIG(release, debug|release) {
  DESTDIR = ${project.build.directory}/bin/release
  OBJECTS_DIR  = ${project.build.directory}/build/release
  MOC_DIR = ${project.build.directory}/build/release/generated
  UI_DIR = ${project.build.directory}/build/release/generated
  RCC_DIR = ${project.build.directory}/build/release/generated
  LIBS = -L${project.build.directory}/build/bin/release -L${environment.dir}/qt/bin/${arch}/release -L${basedir}/../common/${arch}/bin/release $$LIBS
}

win* {
  CONFIG += ${arch}
  LIBS += ${windows.oslibs}
  DEFINES += ${windows.defines}  
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR

    # Don't warn for deprecated 'unsecure' CRT functions.
    DEFINES += _CRT_SECURE_NO_WARNINGS

    # Don't warn for deprecated POSIX functions.
    DEFINES += _CRT_NONSTDC_NO_DEPRECATE 

    # Disable warning C4250: 'Derived' : inherits 'Base::method' via dominance.
    # It is buggy, as described in http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
    QMAKE_CXXFLAGS += /wd4250
  }
  
  !staticlib {
    DEFINES += QN_EXPORT=Q_DECL_EXPORT
  }

  staticlib {
    DEFINES += QN_EXPORT=
  }
}

unix {
  LIBS += ${linux.oslibs}
  DEFINES += QN_EXPORT=
  QMAKE_CXXFLAGS += -msse2
  DEFINES += ${linux.defines}
}

mac {
  DEFINES += QN_EXPORT=  
  LIBS += ${mac.oslibs}
  DEFINES += ${mac.defines}
}