cache()

NAME=${project.artifactId}
BUILDLIB = ${buildLib}
LIBTYPE = ${libtype}
TARGET = ${project.artifactId}
VERSION = ${release.version}
QT += ${qt.libs}
ADDITIONAL_QT_INCLUDES=${environment.dir}/qt5-custom


## GLOBAL CONFIGURATIONS
QMAKE_INFO_PLIST = Info.plist
CONFIG += precompile_header $$BUILDLIB $$LIBTYPE
CONFIG -= flat
DEFINES += USE_NX_HTTP __STDC_CONSTANT_MACROS ${global.defines}
DEFINES += ${additional.defines}
RESOURCES += ${project.build.directory}/build/${project.artifactId}.qrc

android {
  DEFINES += \
    DISABLE_ALL_VENDORS \
    DISABLE_THIRD_PARTY \
    DISABLE_COLDSTORE \
    DISABLE_MDNS \
    DISABLE_ARCHIVE \
    DISABLE_DATA_PROVIDERS \
    DISABLE_SOFTWARE_MOTION_DETECTION \
    DISABLE_SENDMAIL \
    DISABLE_SSL
}

include( optional_functionality.pri )


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

  !win32 {
      contains( DEFINES, debug_in_release ) {
         QMAKE_CXXFLAGS += -ggdb3
         QMAKE_CFLAGS += -ggdb3
         QMAKE_LFLAGS += -ggdb3
      }
      contains( DEFINES, enable_gprof ) {
         QMAKE_CXXFLAGS += -pg
         QMAKE_CFLAGS += -pg
         QMAKE_LFLAGS += -pg
      }
  }
}

win* {
  OUTPUT_PATH = ${libdir}/${arch}
} else {
  OUTPUT_PATH = ${libdir}
}  
    
isEmpty(BUILDLIB) {
  DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION
} else {
    contains(BUILDLIB,staticlib) {
      DESTDIR = $$OUTPUT_PATH/lib/$$CONFIGURATION
    }
    else {
      contains (LIBTYPE,plugin) {
        DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/plugins
      }
      else {
        win* {
          DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION
        }
        else {
          DESTDIR = $$OUTPUT_PATH/lib/$$CONFIGURATION
        }
      }
    }
}

OBJECTS_DIR = ${project.build.directory}/build/$$CONFIGURATION
MOC_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
UI_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
RCC_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
LIBS += -L$$OUTPUT_PATH/lib/$$CONFIGURATION -L${qt.dir}/lib
!win*:!mac {
    LIBS += -Wl,-rpath-link,${qt.dir}/lib
}
LIBS += ${global.libs}

INCLUDEPATH +=  ${qt.dir}/include \
                ${qt.dir}/include/QtCore \
                ${qt.dir}/include/QtZlib \
                ${project.build.sourceDirectory} \
                ${project.build.directory} \
                ${root.dir}/common/src \
                ${libdir}/include \
                ${environment.dir}/include \
                $$ADDITIONAL_QT_INCLUDES \
                ${qt.dir}/include/QtCore/$$QT_VERSION/ \
                ${qt.dir}/include/QtCore/$$QT_VERSION/QtCore/ \

DEPENDPATH *= $${INCLUDEPATH}

PRECOMPILED_HEADER = ${project.build.sourceDirectory}/StdAfx.h
PRECOMPILED_SOURCE = ${project.build.sourceDirectory}/StdAfx.cpp

android: {
  QMAKE_PCH_OUTPUT_EXT    = .gch

  QMAKE_CFLAGS_PRECOMPILE       = -x c-header -c ${QMAKE_PCH_INPUT} -o ${QMAKE_PCH_OUTPUT}
  QMAKE_CFLAGS_USE_PRECOMPILE   = -include ${QMAKE_PCH_OUTPUT_BASE}
  QMAKE_CXXFLAGS_PRECOMPILE     = -x c++-header -c ${QMAKE_PCH_INPUT} -o ${QMAKE_PCH_OUTPUT}
  QMAKE_CXXFLAGS_USE_PRECOMPILE = $$QMAKE_CFLAGS_USE_PRECOMPILE
}

# android:contains(QMAKE_HOST.os,Windows) {
#   # Support precompiled headers on android-mingw
#   QMAKE_CFLAGS_PRECOMPILE       = -x c-header -c ${QMAKE_PCH_INPUT} -o ${QMAKE_PCH_OUTPUT}.gch
#   QMAKE_CFLAGS_USE_PRECOMPILE   = -include ${QMAKE_PCH_OUTPUT}
#   QMAKE_CXXFLAGS_PRECOMPILE     = -x c++-header -c ${QMAKE_PCH_INPUT} -o ${QMAKE_PCH_OUTPUT}.gch
#   QMAKE_CXXFLAGS_USE_PRECOMPILE = $$QMAKE_CFLAGS_USE_PRECOMPILE
# 
#   # Make sure moc files compile
#   QMAKE_CXXFLAGS += -fpermissive
# 
#   # Replace slashes in paths with backslashes
#   OBJECTS_DIR ~= s,/,\\,g
# 
#   # Work around CreateProcess limit on arg size
#   QMAKE_AR_CMD = \
#     del /F $$OBJECTS_DIR\\_list.bat                                                 $$escape_expand(\n\t)\
#                                                                                     $$escape_expand(\n\t)\
#     echo @echo off                          >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#     echo setlocal EnableDelayedExpansion    >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#     echo for %%%%i in (%%1) do (            >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#     echo   set OBJECT=%%%%~fi               >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#     echo   set OBJECT=!OBJECT:\\=/!         >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#     echo   echo !OBJECT!                    >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#     echo )                                  >>$$OBJECTS_DIR\\_list.bat              $$escape_expand(\n\t)\
#                                                                                     $$escape_expand(\n\t)\
#     $$OBJECTS_DIR\\_list.bat $$OBJECTS_DIR\\*.obj >$$OBJECTS_DIR\\_objects.lst      $$escape_expand(\n\t)\
#     $(AR) $(TARGET) @$$OBJECTS_DIR\\_objects.lst
# }


# Workaround for https://bugreports.qt-project.org/browse/QTBUG-29331
QMAKE_MOC_OPTIONS += -DBOOST_MPL_IF_HPP_INCLUDED -DBOOST_TT_TYPE_WITH_ALIGNMENT_INCLUDED -DBOOST_MPL_NOT_HPP_INCLUDED -DBOOST_MPL_VOID_HPP_INCLUDED

CONFIG += ${arch}

win* {
  RC_FILE = ${project.build.directory}/hdwitness.rc
  ICON = ${customization.dir}/icons/hdw_logo.ico	
  LIBS += ${windows.oslibs}
  DEFINES += ${windows.defines}  
  DEFINES += ${global.windows.defines}  
  win32-msvc* {
    # Note on /bigobj: http://stackoverflow.com/questions/15110580/penalty-of-the-msvs-linker-flag-bigobj
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR /bigobj
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
  
  QMAKE_MOC_OPTIONS += -DQ_OS_WIN
}

## LINUX, MAC AND ANDROID
unix {
  DEFINES += QN_EXPORT=  
  QMAKE_CXXFLAGS += -Werror=enum-compare -Werror=reorder -Werror=delete-non-virtual-dtor -Wuninitialized
  arm {
    QMAKE_CXXFLAGS += -std=c++0x 
  } else {
    QMAKE_CXXFLAGS += -std=c++11
  }
}

## LINUX
unix:!android:!mac {
  !arm {
    LIBS += ${linux.oslibs}
    QMAKE_CXXFLAGS += ${compiler.arguments}
    QMAKE_CFLAGS += ${compiler.arguments}
    QMAKE_LFLAGS += ${compiler.arguments}
    QMAKE_CXXFLAGS += -msse2
    QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-local-typedefs
  } else {
    LIBS -= -lssl
    LIBS += ${linux.arm.oslibs}
  } 
  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas -Wno-ignored-qualifiers
  DEFINES += ${linux.defines}
  QMAKE_MOC_OPTIONS += -DQ_OS_LINUX
}

## MAC OS
mac {
  QMAKE_CXXFLAGS += -msse4.1 -mmacosx-version-min=10.7 -stdlib=libc++
  QMAKE_CFLAGS += -msse4.1
  LIBS += ${mac.oslibs}
  DEFINES += ${mac.defines}
  CONFIG -= app_bundle objective_c

  INCLUDEPATH += ${qt.dir}/lib/QtCore.framework/Headers/$$QT_VERSION/QtCore/
}

## ANDROID
android {
  LIBS -= -lssl
  LIBS += ${android.oslibs}

  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas -Wno-ignored-qualifiers
  DEFINES += ${android.defines}
  QMAKE_MOC_OPTIONS += -DQ_OS_LINUX
}












