#include "mobile_client_meta_types.h"

#include <QtQml/QtQml>

#include "context/connection_manager.h"
#include "context/context_settings.h"
#include "ui/color_theme.h"
#include "models/camera_list_model.h"
#include "models/server_list_model.h"
#include "models/login_sessions_model.h"
#include "resources/media_resource_helper.h"
#include "utils/mobile_app_info.h"

void QnMobileClientMetaTypes::initialize() {
    registerQmlTypes();
}

void QnMobileClientMetaTypes::registerQmlTypes() {
    qmlRegisterUncreatableType<QnConnectionManager>("com.networkoptix.qml", 1, 0, "QnConnectionManager", lit("Cannot create an instance of QnConnectionManager."));
    qmlRegisterUncreatableType<QnColorTheme>("com.networkoptix.qml", 1, 0, "QnColorTheme", lit("Cannot create an instance of QnColorTheme."));
    qmlRegisterUncreatableType<QnMobileAppInfo>("com.networkoptix.qml", 1, 0, "QnMobileAppInfo", lit("Cannot create an instance of QnMobileAppInfo."));
    qmlRegisterType<QnCameraListModel>("com.networkoptix.qml", 1, 0, "QnCameraListModel");
    qmlRegisterType<QnServerListModel>("com.networkoptix.qml", 1, 0, "QnServerListModel");
    qmlRegisterType<QnLoginSessionsModel>("com.networkoptix.qml", 1, 0, "QnLoginSessionsModel");
    qmlRegisterType<QnMediaResourceHelper>("com.networkoptix.qml", 1, 0, "QnMediaResourceHelper");
    qmlRegisterType<QnContextSettings>("com.networkoptix.qml", 1, 0, "QnContextSettings");
}
