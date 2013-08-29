TEMPLATE = app
CONFIG += console
QT += core network xml sql concurrent multimedia


include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsinglecoreapplication.pri)
DEFINES += USE_NX_HTTP
