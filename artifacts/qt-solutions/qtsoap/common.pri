exists(config.pri):infile(config.pri, SOLUTIONS_LIBRARY, yes): CONFIG += qtsoap-uselib
TEMPLATE += fakelib
QTSOAP_LIBNAME = $$qtLibraryTarget(QtSolutions_SOAP-head)
TEMPLATE -= fakelib
QTSOAP_LIBDIR = $$PWD/lib
unix:qtsoap-uselib:!qtsoap-buildlib:QMAKE_RPATHDIR += $$QTSOAP_LIBDIR
