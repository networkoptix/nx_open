// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_meta_types.h"

#include <mutex>

#include <QtQml/QtQml>

#include <client_core/client_core_meta_types.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <models/camera_list_model.h>
#include <models/layouts_model.h>
#include <nx/client/mobile/motion/chunk_position_watcher.h>
#include <nx/client/mobile/motion/motion_playback_mask_watcher.h>
#include <nx/client/mobile/two_way_audio/voice_spectrum_item.h>
#include <nx/mobile_client/controllers/audio_controller.h>
#include <nx/mobile_client/controllers/resource_ptz_controller.h>
#include <nx/mobile_client/models/ptz_preset_model.h>
#include <nx/vms/client/core/animation/kinetic_animation.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/watchers/watermark_watcher.h>
#include <nx/vms/client/mobile/camera/buttons/camera_button_controller.h>
#include <nx/vms/client/mobile/camera/buttons/camera_buttons_model.h>
#include <nx/vms/client/mobile/camera/media_download_backend.h>
#include <nx/vms/client/mobile/camera/share_bookmark_backend.h>
#include <nx/vms/client/mobile/event_search/models/parameters_visualization_model.h>
#include <nx/vms/client/mobile/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/mobile/maintenance/remote_log_manager.h>
#include <nx/vms/client/mobile/push_notification/details/push_systems_selection_model.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/push_notification/push_notification_model.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/session/ui_messages.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/client/mobile/utils/navigation_bar_utils.h>
#include <nx/vms/client/mobile/workaround/text_input_workaround.h>
#include <private/qqmlvaluetype_p.h>
#include <resources/camera_access_rights_helper.h>
#include <settings/qml_settings_adaptor.h>
#include <ui/models/ordered_systems_model.h>
#include <ui/models/systems_model.h>
#include <ui/qml/text_input.h>
#include <ui/timeline/timeline.h>
#include <utils/developer_settings_helper.h>
#include <utils/mobile_app_info.h>
#include <watchers/cloud_system_information_watcher.h>
#include <watchers/network_interface_watcher.h>

namespace nx::vms::client::mobile {

using namespace nx::client::mobile;

void registerMetaTypes()
{
    qRegisterMetaType<QnCloudSystemList>();
}

void registerQmlTypes()
{
    qmlRegisterUncreatableType<QnMobileAppInfo>("Nx.Mobile", 1, 0,
        "QnMobileAppInfo", "Cannot create an instance of QnMobileAppInfo.");

    qmlRegisterUncreatableType<nx::client::mobile::QmlSettingsAdaptor>(
        "Nx.Settings", 1, 0, "MobileSettings", "Cannot create an instance of MobileSettings.");

    qmlRegisterType<QnCameraListModel>("Nx.Mobile", 1, 0, "QnCameraListModel");
    qmlRegisterType<QnLayoutsModel>("Nx.Mobile", 1, 0, "QnLayoutsModel");
    qmlRegisterType<core::animation::KineticAnimation>("Nx.Core", 1, 0, "KineticAnimation");
    qmlRegisterType<QnCameraAccessRightsHelper>("Nx.Mobile", 1, 0, "QnCameraAccessRightsHelper");
    qmlRegisterType<QnTimeline>("Nx.Mobile", 1, 0, "QnTimelineView");
    qmlRegisterType<core::CloudStatusWatcher>(
        "nx.vms.client.core", 1, 0, "CloudStatusWatcher");
    qmlRegisterType<QnCloudSystemInformationWatcher>("Nx.Mobile", 1, 0,
        "QnCloudSystemInformationWatcher");
    qmlRegisterUncreatableType<core::UserWatcher>(
        "nx.vms.client.core", 1, 0, "UserWatcher",
        "Use UserWatcher instance from the System Context");

    qmlRegisterType<QnOrderedSystemsModel>("Nx.Models", 1, 0, "OrderedSystemsModel");
    qmlRegisterType<QnQuickTextInput>("Nx.Controls", 1, 0, "TextInput");
    qmlRegisterType<QnMobileClientUiController>("Nx.Mobile", 1, 0, "Controller");
    qmlRegisterType<nx::client::mobile::utils::DeveloperSettingsHelper>(
        "Nx.Settings", 1, 0, "DeveloperSettingsHelper");

    TextInputWorkaround::registerQmlType();

    // Ptz related classes
    qmlRegisterUncreatableType<Ptz>("Nx.Mobile", 1, 0, "Ptz",
        "Cannot create an instance of Ptz class");
    qmlRegisterType<nx::client::mobile::ResourcePtzController>("Nx.Mobile", 1, 0, "PtzController");
    qmlRegisterType<nx::client::mobile::PtzPresetModel>("Nx.Mobile", 1, 0, "PtzPresetModel");
    qmlRegisterUncreatableMetaObject(nx::vms::api::ptz::staticMetaObject, "nx.vms.api", 1, 0,
        "PtzAPI", "PtzAPI is a namespace");

    qmlRegisterType<NetworkInterfaceWatcher>(
        "nx.vms.client.mobile", 1, 0, "NetworkInterfaceWatcher");

    qmlRegisterRevision<QQuickTextInput, 6>("Nx.Controls", 1, 0);
    qmlRegisterRevision<QQuickItem, 1>("Nx.Controls", 1, 0);

    core::CloudUrlHelper::registerSingletonType(
        utils::SystemUri::ReferralSource::MobileClient,
        utils::SystemUri::ReferralContext::WelcomePage);
    core::WatermarkWatcher::registerQmlType();
    nx::client::mobile::VoiceSpectrumItem::registerQmlType();
    nx::client::mobile::MotionPlaybackMaskWatcher::registerQmlType();
    nx::client::mobile::ChunkPositionWatcher::registerQmlType();
    AudioController::registerQmlType();
    PushNotificationManager::registerQmlType();
    details::PushSystemsSelectionModel::registerQmlType();
    NavigationBarUtils::registerQmlType();
    SessionManager::registerQmlType();
    UiMessages::registerQmlType();
    RemoteLogManager::registerQmlType();
    CommonObjectSearchSetup::registerQmlType();
    ParametersVisualizationModel::registerQmlType();
    CameraButtonController::registerQmlType();
    CameraButtonsModel::registerQmlType();
    MediaDownloadBackend::registerQmlType();
    UiController::registerQmlType();
    ShareBookmarkBackend::registerQmlType();
    PushNotificationModel::registerQmlType();

    qmlRegisterUncreatableMetaObject(nx::vms::api::staticMetaObject, "nx.vms.api", 1, 0,
        "API", "API is a namespace");
}

void initializeMetatypesInternal()
{
    core::initializeMetaTypes();
    core::registerQmlTypes();

    registerMetaTypes();
    registerQmlTypes();
}

void initializeMetaTypes()
{
    static std::once_flag initialized;
    std::call_once(initialized, &initializeMetatypesInternal);
}

} // namespace nx::vms::client::mobile
