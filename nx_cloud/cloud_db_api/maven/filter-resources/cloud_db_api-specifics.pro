TEMPLATE = lib
CONFIG += console

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsinglecoreapplication.pri)
include($$ADDITIONAL_QT_INCLUDES/qtservice/src/qtservice.pri)

INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_api/src/include/
