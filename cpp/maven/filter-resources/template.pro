cache()

NAME=${project.artifactId}
BUILDLIB = ${buildLib}
LIBTYPE = ${libtype}
TEMPLATE = ${template}
TARGET = ${targetName}
GUID = ${guid}
VERSION = ${release.version}
QT = ${qt.libs}
BOX = ${box}
ROOT_DIR = $$clean_path("${root.dir}")

CONFIG += unversioned_soname unversioned_libname

## GLOBAL CONFIGURATIONS
!ios|equals(TEMPLATE, app) {
    CONFIG += precompile_header
} else {
    QMAKE_CXXFLAGS += -include ${project.build.sourceDirectory}/StdAfx.h
}
CONFIG += $$BUILDLIB $$LIBTYPE
CONFIG -= flat
CONFIG += no_private_qt_headers_warning
CONFIG += c++14
DEFINES += USE_NX_HTTP __STDC_CONSTANT_MACROS ${global.defines}
DEFINES += ${customization.defines}
DEFINES += ${additional.defines}
RESOURCES += ${project.build.directory}/build/${project.artifactId}.qrc

if (android | ios) {
  DEFINES += \
    DISABLE_ALL_VENDORS \
    DISABLE_THIRD_PARTY \
    DISABLE_COLDSTORE \
    DISABLE_MDNS \
    DISABLE_DATA_PROVIDERS \
    DISABLE_SOFTWARE_MOTION_DETECTION \
    DISABLE_SENDMAIL
}

include( optional_functionality.pri )

#Warning: enabling ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK can significantly reduce performance
#DEFINES += USE_OWN_MUTEX ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK

CONFIG(debug, debug|release) {
  CONFIGURATION=debug
  isEmpty(BUILDLIB) {
    CONFIG += console
  }
  !win* {
    DEFINES += _DEBUG
  }
  !linux-clang {
    # Temporary fix for linux clang 3.6-3.7 that crashes with our mutex.
    DEFINES += USE_OWN_MUTEX
  }
  CONFIG += qml_debug
}
else {
  CONFIGURATION=release

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

OUTPUT_PATH = $$clean_path("${libdir}")

isEmpty(BUILDLIB) {
  DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/
} else {
    contains(BUILDLIB,staticlib) {
      DESTDIR = $$OUTPUT_PATH/lib/$$CONFIGURATION/
    }
    else {
      contains (LIBTYPE,plugin) {
        DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/plugins
      }
      else {
        win* {
          DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/
        }
        else {
          DESTDIR = $$OUTPUT_PATH/lib/$$CONFIGURATION/
        }
      }
    }
}

OBJECTS_DIR = ${project.build.directory}/build/$$CONFIGURATION/
MOC_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
UI_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated
RCC_DIR = ${project.build.directory}/build/$$CONFIGURATION/generated

LIBS += -L$$OUTPUT_PATH/lib -L$$OUTPUT_PATH/lib/$$CONFIGURATION -L$$OUTPUT_PATH/bin/$$CONFIGURATION
!win*:!mac {
    LIBS += -Wl,-rpath-link,$$clean_path("${qt.dir}")/lib
    LIBS += -Wl,-rpath-link,$$OUTPUT_PATH/lib/$$CONFIGURATION
}
LIBS += ${global.libs}
LIBS += ${subdirectory.global.libs}

INCLUDEPATH +=  ${project.build.sourceDirectory} \
                ${project.build.directory} \
                $$ROOT_DIR/common/src \
                $$ROOT_DIR/common_libs/nx_network/src \
                $$ROOT_DIR/common_libs/nx_fusion/src \
                $$ROOT_DIR/common_libs/nx_utils/src \
                $$ROOT_DIR/common_libs/nx_media/src \
                $$ROOT_DIR/common_libs/nx_audio/src \
                ${packages.dir}/any/nx_kit/src \
                $$clean_path("${libdir}")/include \
                $$ADDITIONAL_QT_INCLUDES

win* {
    DEFINES += \
        NX_KIT_API=__declspec(dllimport) \
        NX_NETWORK_API=__declspec(dllimport) \
        NX_UTILS_API=__declspec(dllimport) \
        NX_FUSION_API=__declspec(dllimport) \
        NX_VMS_UTILS_API=__declspec(dllimport) \
        UDT_API=__declspec(dllimport) \

} else {
    DEFINES += \
        NX_KIT_API= \
        NX_NETWORK_API= \
        NX_UTILS_API= \
        NX_FUSION_API= \
        NX_VMS_UTILS_API= \
        UDT_API= \

}

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
  ICON = ${customization.dir}/icons/all/favicon.ico
  LIBS += ${windows.oslibs}
  DEFINES += NOMINMAX= ${windows.defines}
  DEFINES += ${global.windows.defines}
  win32-msvc* {
    # Note on /bigobj: http://stackoverflow.com/questions/15110580/penalty-of-the-msvs-linker-flag-bigobj
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR /bigobj /wd4290 /wd4661 /wd4100 /we4717
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

## BOTH LINUX AND MAC
unix: {
  DEFINES += QN_EXPORT=
  clang {
    QMAKE_CXXFLAGS += -Wno-c++14-extensions -Wno-inconsistent-missing-override
  } else {
    #QMAKE_CXXFLAGS += -std=c++1y
  }
  QMAKE_CXXFLAGS += -Werror=enum-compare -Werror=reorder -Werror=delete-non-virtual-dtor -Werror=return-type -Werror=conversion-null -Wuninitialized -Wno-error=maybe-uninitialized
}

!win32 {
  ext_debug2.target  = $(DESTDIR)$(TARGET).debug
  ext_debug2.depends = $(DESTDIR)$(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(DESTDIR)/$(TARGET) $(DESTDIR)/$(TARGET).debug; $(STRIP) -g $(DESTDIR)/$(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(DESTDIR)/$(TARGET).debug $(DESTDIR)/$(TARGET); touch $(DESTDIR)/$(TARGET).debug

  ext_debug.depends = $(DESTDIR)$(TARGET).debug

  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}

## LINUX
linux*:!android {
  !arm {
    LIBS += ${linux.oslibs}
    !clang: {
        QMAKE_CXXFLAGS += -msse2
    } else {
        QMAKE_CXXFLAGS += -msse4.1
    }
    QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-local-typedefs
    QMAKE_CXXFLAGS += -fstack-protector-all
  } else {
    LIBS -= -lssl
    LIBS += ${linux.arm.oslibs}
    QMAKE_CXXFLAGS += -fno-omit-frame-pointer
    CONFIG(release, debug|release)|!equals(BOX, tx1): QMAKE_CXXFLAGS += -ggdb1
  }
  QMAKE_LFLAGS += -rdynamic
  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas -Wno-ignored-qualifiers
  DEFINES += ${linux.defines}
  QMAKE_MOC_OPTIONS += -DQ_OS_LINUX

  equals(TEMPLATE, app): QMAKE_RPATHDIR += $ORIGIN/../lib
  contains(TEMPLATE, "lib"): LIBS += "-Wl,--allow-shlib-undefined"
}

## MAC OS
macx {
  QMAKE_INFO_PLIST = Info.plist
  QMAKE_CXXFLAGS += -msse4.1 -mmacosx-version-min=10.8 -stdlib=libc++
  QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.8
  QMAKE_CFLAGS += -msse4.1
  QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-local-typedef
  LIBS += ${mac.oslibs}
  DEFINES += ${mac.defines}
  CONFIG -= app_bundle objective_c

  contains(TEMPLATE, "lib") {
    QMAKE_LFLAGS += -undefined dynamic_lookup
  }
}

## ANDROID
android {
  LIBS -= -lssl
  LIBS += ${android.oslibs}

  QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas -Wno-ignored-qualifiers
  # Android mkspec ignores CONFIG+=c++14 and forces -std=c++11.
  # Replacing the parameter manually.
  QMAKE_CXXFLAGS -= -std=c++11
  QMAKE_CXXFLAGS += -std=c++1y
  DEFINES += ${android.defines}
  QMAKE_MOC_OPTIONS += -DQ_OS_LINUX
  CONFIG += no_smart_library_merge
}

## iOS
ios {
    LIBS += ${ios.oslibs}
    DEFINES += ${ios.defines}
    QMAKE_MOC_OPTIONS += -DQ_OS_IOS
    QMAKE_IOS_DEPLOYMENT_TARGET = 8.0
    XCODEBUILD_FLAGS += -jobs 4
    QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-local-typedef
}



CONFIG(debug, debug|release) {
  include(dependencies-debug.pri)
} else {
  include(dependencies.pri)
}

