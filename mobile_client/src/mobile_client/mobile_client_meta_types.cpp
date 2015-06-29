#include "mobile_client_meta_types.h"

#include <QtQml/QtQml>

#include "context/connection_manager.h"
#include "context/context_settings.h"
#include "context/login_session_manager.h"
#include "ui/color_theme.h"
#include "ui/timeline/timeline.h"
#include "models/camera_list_model.h"
#include "models/server_list_model.h"
#include "models/login_sessions_model.h"
#include "models/calendar_model.h"
#include "resources/media_resource_helper.h"
#include "utils/mobile_app_info.h"
#include "mobile_client/mobile_client_settings.h"
#include "camera/camera_chunk_provider.h"

void QnMobileClientMetaTypes::initialize() {
    registerMetaTypes();
    registerQmlTypes();
}

void QnMobileClientMetaTypes::registerMetaTypes() {
    qRegisterMetaType<QnAspectRatioHash>();
    qRegisterMetaTypeStreamOperators<QnAspectRatioHash>();
}

void QnMobileClientMetaTypes::registerQmlTypes() {
    qmlRegisterUncreatableType<QnConnectionManager>("com.networkoptix.qml", 1, 0, "QnConnectionManager", lit("Cannot create an instance of QnConnectionManager."));
    qmlRegisterUncreatableType<QnLoginSessionManager>("com.networkoptix.qml", 1, 0, "QnLoginSessionManager", lit("Cannot create an instance of QnLoginSessionManager."));
    qmlRegisterUncreatableType<QnColorTheme>("com.networkoptix.qml", 1, 0, "QnColorTheme", lit("Cannot create an instance of QnColorTheme."));
    qmlRegisterUncreatableType<QnMobileAppInfo>("com.networkoptix.qml", 1, 0, "QnMobileAppInfo", lit("Cannot create an instance of QnMobileAppInfo."));
    qmlRegisterUncreatableType<QnContextSettings>("com.networkoptix.qml", 1, 0, "QnContextSettings", lit("Cannot create an instance of QnContextSettings."));
    qmlRegisterType<QnCameraListModel>("com.networkoptix.qml", 1, 0, "QnCameraListModel");
    qmlRegisterType<QnServerListModel>("com.networkoptix.qml", 1, 0, "QnServerListModel");
    qmlRegisterType<QnCalendarModel>("com.networkoptix.qml", 1, 0, "QnCalendarModel");
    qmlRegisterType<QnLoginSessionsModel>("com.networkoptix.qml", 1, 0, "QnLoginSessionsModel");
    qmlRegisterType<QnMediaResourceHelper>("com.networkoptix.qml", 1, 0, "QnMediaResourceHelper");
    qmlRegisterType<QnTimeline>("com.networkoptix.qml", 1, 0, "QnTimelineView");
    qmlRegisterType<QnCameraChunkProvider>("com.networkoptix.qml", 1, 0, "QnCameraChunkProvider");

    qmlRegisterSingletonType(QUrl(lit("qrc:///qml/QnTheme.qml")), "com.networkoptix.qml", 1, 0, "QnTheme");
}
