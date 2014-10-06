#include "mobile_client_meta_types.h"

#include <QtQml/QtQml>

#include "context/connection_manager.h"
#include "ui/color_theme.h"

void QnMobileClientMetaTypes::initialize() {
    registerQmlTypes();
}

void QnMobileClientMetaTypes::registerQmlTypes() {
    qmlRegisterUncreatableType<QnConnectionManager>("com.networkoptix.qml", 1, 0, "QnConnectionManager", lit("Cannot create an instance of QnConnectionManager."));
    qmlRegisterUncreatableType<QnColorTheme>("com.networkoptix.qml", 1, 0, "QnColorTheme", lit("Cannot create an instance of QnColorTheme."));
}
