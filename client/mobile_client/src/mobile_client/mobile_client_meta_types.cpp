#include "mobile_client_meta_types.h"

#include <QtQml/QtQml>
#include <private/qqmlvaluetype_p.h>

#include <nx/fusion/model_functions.h>

#include <context/connection_manager.h>
#include <ui/timeline/timeline.h>
#include <ui/qml/text_input.h>
#include <ui/models/systems_model.h>
#include <models/camera_list_model.h>
#include <models/calendar_model.h>
#include <models/layouts_model.h>
#include <resources/resource_helper.h>
#include <resources/media_resource_helper.h>
#include <resources/camera_access_rights_helper.h>
#include <utils/mobile_app_info.h>
#include <mobile_client/mobile_client_settings.h>
#include <camera/camera_chunk_provider.h>
#include <camera/active_camera_thumbnail_loader.h>
#include <camera/thumbnail_cache_accessor.h>
#include <nx/mobile_client/media/media_player.h>
#include <watchers/cloud_status_watcher.h>
#include <watchers/cloud_system_information_watcher.h>
#include <watchers/user_watcher.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <client_core/client_core_meta_types.h>
#include <controllers/lite_client_controller.h>
#include <helpers/cloud_url_helper.h>
#include <utils/developer_settings_helper.h>
#include <settings/qml_settings_adaptor.h>
#include <nx/mobile_client/helpers/inter_client_message.h>
#include <nx/mobile_client/controllers/resource_ptz_controller.h>
#include <nx/mobile_client/models/ptz_preset_model.h>
#include <nx/client/core/resource/layout_accessor.h>
#include <nx/client/core/animation/kinetic_animation.h>
#include <nx/client/mobile/resource/lite_client_layout_helper.h>

using namespace nx::client::mobile;

void QnMobileClientMetaTypes::initialize()
{
    nx::client::core::initializeMetaTypes();

    registerMetaTypes();
    registerQmlTypes();
}

void QnMobileClientMetaTypes::registerMetaTypes()
{
    qRegisterMetaType<nx::media::PlayerStatistics>();
    QnJsonSerializer::registerSerializer<InterClientMessage::Command>();
}

void QnMobileClientMetaTypes::registerQmlTypes() {
    qmlRegisterUncreatableType<QnConnectionManager>("com.networkoptix.qml", 1, 0, "QnConnectionManager", lit("Cannot create an instance of QnConnectionManager."));
    qmlRegisterUncreatableType<QnMobileAppInfo>("com.networkoptix.qml", 1, 0, "QnMobileAppInfo", lit("Cannot create an instance of QnMobileAppInfo."));
    qmlRegisterUncreatableType<QnCloudUrlHelper>("com.networkoptix.qml", 1, 0, "QnCloudUrlHelper", lit("Cannot create an instance of QnCloudUrlHelper."));
    qmlRegisterUncreatableType<nx::client::mobile::QmlSettingsAdaptor>(
        "Nx.Settings", 1, 0, "MobileSettings",
        lit("Cannot create an instance of MobileSettings."));

    qmlRegisterType<QnCameraListModel>("com.networkoptix.qml", 1, 0, "QnCameraListModel");
    qmlRegisterType<QnCalendarModel>("com.networkoptix.qml", 1, 0, "QnCalendarModel");
    qmlRegisterType<QnLayoutsModel>("com.networkoptix.qml", 1, 0, "QnLayoutsModel");
    qmlRegisterType<QnResourceHelper>("Nx.Core", 1, 0, "ResourceHelper");
    qmlRegisterType<QnMediaResourceHelper>("Nx.Core", 1, 0, "MediaResourceHelper");
    qmlRegisterType<nx::client::core::resource::LayoutAccessor>("Nx.Core", 1, 0, "LayoutAccessor");
    qmlRegisterType<nx::client::core::animation::KineticAnimation>("Nx.Core", 1, 0, "KineticAnimation");
    qmlRegisterType<QnCameraAccessRightsHelper>("com.networkoptix.qml", 1, 0, "QnCameraAccessRightsHelper");
    qmlRegisterType<QnTimeline>("com.networkoptix.qml", 1, 0, "QnTimelineView");
    qmlRegisterType<QnCameraChunkProvider>("com.networkoptix.qml", 1, 0, "QnCameraChunkProvider");
    qmlRegisterType<QnCloudStatusWatcher>("com.networkoptix.qml", 1, 0, "QnCloudStatusWatcher");
    qmlRegisterType<QnCloudSystemInformationWatcher>("com.networkoptix.qml", 1, 0, "QnCloudSystemInformationWatcher");
    qmlRegisterType<QnUserWatcher>("com.networkoptix.qml", 1, 0, "QnUserWatcher");
    /* NxMediaPlayer should not be used.
       It is here only to allow assignments of MediaPlayer to properties of this type. */
    qmlRegisterType<MediaPlayer>("Nx.Media", 1, 0, "MediaPlayer");
    qmlRegisterUncreatableType<nx::media::Player>("Nx.Media", 1, 0, "NxMediaPlayer", lit("Cannot create an instance of abstract class."));
    qmlRegisterType<QnActiveCameraThumbnailLoader>("com.networkoptix.qml", 1, 0, "QnActiveCameraThumbnailLoader");
    qmlRegisterType<QnThumbnailCacheAccessor>("com.networkoptix.qml", 1, 0, "QnThumbnailCacheAccessor");
    qmlRegisterType<QnQuickTextInput>("Nx.Controls", 1, 0, "TextInput");
    qmlRegisterType<QnMobileClientUiController>("com.networkoptix.qml", 1, 0, "QnMobileClientUiController");
    qmlRegisterType<QnLiteClientController>("com.networkoptix.qml", 1, 0, "QnLiteClientController");
    qmlRegisterType<resource::LiteClientLayoutHelper>("Nx.Core", 1, 0, "LiteClientLayoutHelper");
    qmlRegisterType<utils::DeveloperSettingsHelper>(
        "Nx.Settings", 1, 0, "DeveloperSettingsHelper");

    // Ptz related classes
    qmlRegisterUncreatableType<Ptz>("Nx.Core", 1, 0, "Ptz",
        lit("Cannot create an instance of Ptz class"));
    qmlRegisterType<nx::client::mobile::ResourcePtzController>("Nx.Core", 1, 0, "PtzController");
    qmlRegisterType<nx::client::mobile::PtzPresetModel>("Nx.Core", 1, 0, "PtzPresetModel");

    qmlRegisterRevision<QQuickTextInput, 6>("Nx.Controls", 1, 0);
    qmlRegisterRevision<QQuickItem, 1>("Nx.Controls", 1, 0);
    qmlRegisterRevision<QQuickItem, 1>("com.networkoptix.qml", 1, 0);

    qmlRegisterSingletonType(QUrl(lit("qrc:///qml/QnTheme.qml")), "com.networkoptix.qml", 1, 0, "QnTheme");
}
