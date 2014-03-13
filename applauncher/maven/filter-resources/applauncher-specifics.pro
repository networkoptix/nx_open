TEMPLATE = app

QMAKE_RPATHDIR = ""
!win32 {
     QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsingleapplication.pri)
