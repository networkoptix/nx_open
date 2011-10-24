CONFIG += x86
QT += network xml
QT -= gui

TARGET = consoleapp
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += SessionManager.cpp Objects.cpp Main.cpp
HEADERS += SessionManager.h Objects.h Main.h \
    AppServerConnection.h

QMAKE_CXXFLAGS += -I/Users/ivan/opt/include -I/Users/ivan/sdk/xerces-c-3.1.1-x86-macosx-gcc-4.0/include
LIBS += -L/Users/ivan/sdk/xerces-c-3.1.1-x86-macosx-gcc-4.0/lib -lxerces-c

XSD_FILES = xsd/cameras.xsd xsd/layouts.xsd xsd/resourceTypes.xsd xsd/resources.xsd xsd/servers.xsd xsd/events.xsd

xsd.name = Generating code from ${QMAKE_FILE_IN}
xsd.input = XSD_FILES
xsd.output = generated/${QMAKE_FILE_BASE}.cpp
xsd.commands = xsd cxx-tree --output-dir generated --cxx-suffix .cpp --hxx-suffix .h --generate-ostream --root-element ${QMAKE_FILE_BASE} ${QMAKE_FILE_IN}
xsd.CONFIG += target_predeps
xsd.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += xsd

