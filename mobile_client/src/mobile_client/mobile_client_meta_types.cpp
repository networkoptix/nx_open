#include "mobile_client_meta_types.h"

#include <QtQml/QtQml>

#include <context/connection_manager.h>
#include <context/context_settings.h>
#include <context/session_settings.h>
#include <ui/timeline/timeline.h>
#include <ui/qml/quick_item_mouse_tracker.h>
#include <ui/qml/text_input.h>
#include <ui/models/systems_model.h>
#include <ui/models/recent_user_connections_model.h>
#include <ui/models/system_hosts_model.h>
#include <models/camera_list_model.h>
#include <models/saved_sessions_model.h>
#include <models/discovered_sessions_model.h>
#include <models/calendar_model.h>
#include <models/layouts_model.h>
#include <resources/media_resource_helper.h>
#include <resources/camera_access_rights_helper.h>
#include <utils/mobile_app_info.h>
#include <utils/mjpeg/mjpeg_player.h>
#include <mobile_client/mobile_client_settings.h>
#include <camera/camera_chunk_provider.h>
#include <camera/active_camera_thumbnail_loader.h>
#include <nx/media/media_player.h>
#include <watchers/cloud_status_watcher.h>

void QnMobileClientMetaTypes::initialize() {
    registerMetaTypes();
    registerQmlTypes();
}

void QnMobileClientMetaTypes::registerMetaTypes() {
    qRegisterMetaType<QnAspectRatioHash>();
    qRegisterMetaTypeStreamOperators<QnAspectRatioHash>();
    qRegisterMetaType<QnStringSet>();
    qRegisterMetaTypeStreamOperators<QnStringSet>();
}

void QnMobileClientMetaTypes::registerQmlTypes() {
    qmlRegisterUncreatableType<QnConnectionManager>("com.networkoptix.qml", 1, 0, "QnConnectionManager", lit("Cannot create an instance of QnConnectionManager."));
    qmlRegisterUncreatableType<QnMobileAppInfo>("com.networkoptix.qml", 1, 0, "QnMobileAppInfo", lit("Cannot create an instance of QnMobileAppInfo."));
    qmlRegisterUncreatableType<QnContextSettings>("com.networkoptix.qml", 1, 0, "QnContextSettings", lit("Cannot create an instance of QnContextSettings."));
    qmlRegisterType<QnSystemsModel>("com.networkoptix.qml", 1, 0, "QnSystemsModel");
    qmlRegisterType<QnRecentUserConnectionsModel>("com.networkoptix.qml", 1, 0, "QnRecentUserConnectionsModel");
    qmlRegisterType<QnSystemHostsModel>("com.networkoptix.qml", 1, 0, "QnSystemHostsModel");
    qmlRegisterType<QnCameraListModel>("com.networkoptix.qml", 1, 0, "QnCameraListModel");
    qmlRegisterType<QnCalendarModel>("com.networkoptix.qml", 1, 0, "QnCalendarModel");
    qmlRegisterType<QnLayoutsModel>("com.networkoptix.qml", 1, 0, "QnLayoutsModel");
    qmlRegisterType<QnSavedSessionsModel>("com.networkoptix.qml", 1, 0, "QnSavedSessionsModel");
    qmlRegisterType<QnDiscoveredSessionsModel>("com.networkoptix.qml", 1, 0, "QnDiscoveredSessionsModel");
    qmlRegisterType<QnMediaResourceHelper>("com.networkoptix.qml", 1, 0, "QnMediaResourceHelper");
    qmlRegisterType<QnCameraAccessRightsHelper>("com.networkoptix.qml", 1, 0, "QnCameraAccessRightsHelper");
    qmlRegisterType<QnTimeline>("com.networkoptix.qml", 1, 0, "QnTimelineView");
    qmlRegisterType<QnCameraChunkProvider>("com.networkoptix.qml", 1, 0, "QnCameraChunkProvider");
    qmlRegisterType<QnMjpegPlayer>("com.networkoptix.qml", 1, 0, "QnMjpegPlayer");
    qmlRegisterType<QnCloudStatusWatcher>("com.networkoptix.qml", 1, 0, "QnCloudStatusWatcher");
    qmlRegisterType<nx::media::Player>("com.networkoptix.qml", 1, 0, "QnPlayer");
    qmlRegisterType<QnActiveCameraThumbnailLoader>("com.networkoptix.qml", 1, 0, "QnActiveCameraThumbnailLoader");
    qmlRegisterType<QnQuickItemMouseTracker>("com.networkoptix.qml", 1, 0, "ItemMouseTracker");
    qmlRegisterType<QnQuickTextInput>("com.networkoptix.qml", 1, 0, "QnTextInput");

    qmlRegisterRevision<QQuickTextInput, 6>("com.networkoptix.qml", 1, 0);
    qmlRegisterRevision<QQuickItem, 1>("com.networkoptix.qml", 1, 0);

    qmlRegisterSingletonType(QUrl(lit("qrc:///qml/QnTheme.qml")), "com.networkoptix.qml", 1, 0, "QnTheme");
}
