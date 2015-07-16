TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsinglecoreapplication.pri)
include($$ADDITIONAL_QT_INCLUDES/qtservice/src/qtservice.pri)
