#include "mobile_client_meta_types.h"

#include <QtQml/QtQml>
#include <private/qqmlvaluetype_p.h>

#include <context/connection_manager.h>
#include <ui/timeline/timeline.h>
#include <ui/qml/quick_item_mouse_tracker.h>
#include <ui/qml/text_input.h>
#include <ui/models/systems_model.h>
#include <models/camera_list_model.h>
#include <models/calendar_model.h>
#include <models/layouts_model.h>
#include <resources/media_resource_helper.h>
#include <resources/camera_access_rights_helper.h>
#include <utils/mobile_app_info.h>
#include <mobile_client/mobile_client_settings.h>
#include <camera/camera_chunk_provider.h>
#include <camera/active_camera_thumbnail_loader.h>
#include <camera/thumbnail_cache_accessor.h>
#include <nx/media/media_player.h>
#include <watchers/cloud_status_watcher.h>
#include <watchers/cloud_system_information_watcher.h>
#include <watchers/user_watcher.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <client_core/client_core_meta_types.h>
#include <controllers/lite_client_controller.h>
#include <helpers/lite_client_layout_helper.h>
#include <helpers/cloud_url_helper.h>
#include <utils/developer_settings_helper.h>

using namespace nx::client::mobile;

void QnMobileClientMetaTypes::initialize()
{
    QnClientCoreMetaTypes::initialize();

    registerMetaTypes();
    registerQmlTypes();
}

void QnMobileClientMetaTypes::registerMetaTypes()
{
    qRegisterMetaType<nx::media::PlayerStatistics>();
}

void QnMobileClientMetaTypes::registerQmlTypes() {
    qmlRegisterUncreatableType<QnConnectionManager>("com.networkoptix.qml", 1, 0, "QnConnectionManager", lit("Cannot create an instance of QnConnectionManager."));
    qmlRegisterUncreatableType<QnMobileAppInfo>("com.networkoptix.qml", 1, 0, "QnMobileAppInfo", lit("Cannot create an instance of QnMobileAppInfo."));
    qmlRegisterUncreatableType<QnCloudUrlHelper>("com.networkoptix.qml", 1, 0, "QnCloudUrlHelper", lit("Cannot create an instance of QnCloudUrlHelper."));
    qmlRegisterType<QnCameraListModel>("com.networkoptix.qml", 1, 0, "QnCameraListModel");
    qmlRegisterType<QnCalendarModel>("com.networkoptix.qml", 1, 0, "QnCalendarModel");
    qmlRegisterType<QnLayoutsModel>("com.networkoptix.qml", 1, 0, "QnLayoutsModel");
    qmlRegisterType<QnMediaResourceHelper>("com.networkoptix.qml", 1, 0, "QnMediaResourceHelper");
    qmlRegisterType<QnCameraAccessRightsHelper>("com.networkoptix.qml", 1, 0, "QnCameraAccessRightsHelper");
    qmlRegisterType<QnTimeline>("com.networkoptix.qml", 1, 0, "QnTimelineView");
    qmlRegisterType<QnCameraChunkProvider>("com.networkoptix.qml", 1, 0, "QnCameraChunkProvider");
    qmlRegisterType<QnCloudStatusWatcher>("com.networkoptix.qml", 1, 0, "QnCloudStatusWatcher");
    qmlRegisterType<QnCloudSystemInformationWatcher>("com.networkoptix.qml", 1, 0, "QnCloudSystemInformationWatcher");
    qmlRegisterType<QnUserWatcher>("com.networkoptix.qml", 1, 0, "QnUserWatcher");
    qmlRegisterType<nx::media::Player>("com.networkoptix.qml", 1, 0, "QnPlayer");
    qmlRegisterType<QnActiveCameraThumbnailLoader>("com.networkoptix.qml", 1, 0, "QnActiveCameraThumbnailLoader");
    qmlRegisterType<QnThumbnailCacheAccessor>("com.networkoptix.qml", 1, 0, "QnThumbnailCacheAccessor");
    qmlRegisterType<QnQuickItemMouseTracker>("com.networkoptix.qml", 1, 0, "ItemMouseTracker");
    qmlRegisterType<QnQuickTextInput>("com.networkoptix.qml", 1, 0, "QnTextInput");
    qmlRegisterType<QnMobileClientUiController>("com.networkoptix.qml", 1, 0, "QnMobileClientUiController");
    qmlRegisterType<QnLiteClientController>("com.networkoptix.qml", 1, 0, "QnLiteClientController");
    qmlRegisterType<QnLiteClientLayoutHelper>("com.networkoptix.qml", 1, 0, "QnLiteClientLayoutHelper");
    qmlRegisterType<utils::DeveloperSettingsHelper>(
        "com.networkoptix.qml", 1, 0, "DeveloperSettingsHelper");

    qmlRegisterRevision<QQuickTextInput, 6>("com.networkoptix.qml", 1, 0);
    qmlRegisterRevision<QQuickItem, 1>("com.networkoptix.qml", 1, 0);

    qmlRegisterSingletonType(QUrl(lit("qrc:///qml/QnTheme.qml")), "com.networkoptix.qml", 1, 0, "QnTheme");
}
