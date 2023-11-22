// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_meta_types.h"

#include <atomic>

#include <QtCore/QAbstractItemModel>
#include <QtQml/QtQml>

#include <qt_helpers/scene_position_listener.h>

#include <client/forgotten_systems_manager.h>
#include <common/common_meta_types.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/software_version.h>
#include <nx/vms/api/types/dewarping_types.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/animation/kinetic_animation.h>
#include <nx/vms/client/core/camera/buttons/abstract_camera_button_controller.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <nx/vms/client/core/common/helpers/texture_size_helper.h>
#include <nx/vms/client/core/common/utils/collator.h>
#include <nx/vms/client/core/common/utils/encoded_credentials.h>
#include <nx/vms/client/core/common/utils/path_util.h>
#include <nx/vms/client/core/common/utils/properties_sync.h>
#include <nx/vms/client/core/common/utils/property_update_filter.h>
#include <nx/vms/client/core/common/utils/validators.h>
#include <nx/vms/client/core/common/utils/velocity_meter.h>
#include <nx/vms/client/core/graphics/shader_helper.h>
#include <nx/vms/client/core/items/grid_viewport.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/motion/helpers/camera_motion_helper.h>
#include <nx/vms/client/core/motion/helpers/media_player_motion_provider.h>
#include <nx/vms/client/core/motion/items/motion_mask_item.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/core/network/server_certificate_validation_level_metatype.h>
#include <nx/vms/client/core/qml/nx_globals_object.h>
#include <nx/vms/client/core/qml/positioners/grid_positioner.h>
#include <nx/vms/client/core/qml/qml_test_helper.h>
#include <nx/vms/client/core/resource/access_helper.h>
#include <nx/vms/client/core/resource/media_dewarping_params.h>
#include <nx/vms/client/core/resource/media_resource_helper.h>
#include <nx/vms/client/core/resource/resource_helper.h>
#include <nx/vms/client/core/settings/global_temporaries.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/font_config.h>
#include <nx/vms/client/core/thumbnails/abstract_resource_thumbnail.h>
#include <nx/vms/client/core/time/calendar_model.h>
#include <nx/vms/client/core/time/date_range.h>
#include <nx/vms/client/core/time/day_hours_model.h>
#include <nx/vms/client/core/time/display_time_helper.h>
#include <nx/vms/client/core/time/month_list_model.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/client/core/ui/frame_section.h>
#include <nx/vms/client/core/utils/file_io.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/utils/model_item_flags_watcher.h>
#include <nx/vms/client/core/utils/persistent_index_watcher.h>
#include <nx/vms/client/core/utils/quick_item_mouse_tracker.h>
#include <nx/vms/rules/metatypes.h>
#include <ui/models/authentication_data_model.h>
#include <ui/models/model_data_accessor.h>
#include <ui/models/system_hosts_model.h>

namespace nx::vms::client::core {

namespace {

static QObject* createNxGlobals(QQmlEngine*, QJSEngine*)
{
    return new NxGlobalsObject();
}

} // namespace

void initializeMetaTypes()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    QnCommonMetaTypes::initialize();
    nx::vms::rules::Metatypes::initialize();

    QnJsonSerializer::registerSerializer<EncodedCredentials>();

    qRegisterMetaType<nx::media::PlayerStatistics>();

    qRegisterMetaType<MotionSelection>();

    // To create properties of type QAbstractItemModel* in QML and to assign C++ object properties
    // of that type from QML.
    qmlRegisterUncreatableType<QAbstractItemModel>("Nx", 1, 0, "AbstractItemModel",
        "Cannot create instance of AbstractItemModel.");

    qmlRegisterType<QmlTestHelper>("Nx.Test", 1, 0, "QmlTestHelper");
    qmlRegisterType<ScenePositionListener>("nx.vms.client.core", 1, 0, "ScenePositionListener");
    qmlRegisterType<ResourceHelper>("Nx.Core", 1, 0, "ResourceHelper");
    qmlRegisterType<MediaResourceHelper>("Nx.Core", 1, 0, "MediaResourceHelper");

    qmlRegisterType<AuthenticationDataModel>("Nx.Models", 1, 0, "AuthenticationDataModel");
    qmlRegisterType<QnSystemHostsModel>("Nx.Models", 1, 0, "SystemHostsModel");
    qmlRegisterType<nx::client::ModelDataAccessor>("Nx.Models", 1, 0, "ModelDataAccessor");

    qmlRegisterType<positioners::Grid>("Nx.Positioners", 1, 0, "Grid");

    animation::KineticAnimation::registerQmlType();

    qmlRegisterType<Collator>("Nx.Core", 1, 0, "Collator");

    qmlRegisterSingletonType<NxGlobalsObject>("Nx", 1, 0, "NxGlobals", &createNxGlobals);
    qmlRegisterUncreatableType<QnUuid>(
        "Nx.Utils", 1, 0, "Uuid", "Cannot create an instance of Uuid.");
    qmlRegisterUncreatableType<utils::Url>(
        "Nx.Utils", 1, 0, "Url", "Cannot create an instance of Url.");
    qmlRegisterUncreatableType<nx::utils::SoftwareVersion>(
        "Nx", 1, 0, "SoftwareVersion", "Cannot create an instance of SoftwareVersion.");
    qRegisterMetaType<DateRange>();
    qmlRegisterUncreatableType<DateRange>(
        "nx.vms.client.core", 1, 0, "DateRange", "Cannot create an instance of DateRange.");

    FrameSection::registedQmlType();
    Geometry::registerQmlType();
    QuickItemMouseTracker::registerQmlType();
    CameraMotionHelper::registerQmlType();
    MediaPlayerMotionProvider::registerQmlType();
    ModelItemFlagsWatcher::registerQmlType();
    MotionMaskItem::registerQmlType();
    PropertiesSync::registerQmlTypes();
    PropertyUpdateFilter::registerQmlType();
    PathUtil::registerQmlType();
    TextureSizeHelper::registerQmlType();
    IntValidator::registerQmlType();
    DoubleValidator::registerQmlType();
    graphics::ShaderHelper::registerQmlType();
    VelocityMeter::registerQmlType();
    AbstractResourceThumbnail::registerQmlType();
    PersistentIndexWatcher::registerQmlType();
    FileIO::registerQmlType();
    DisplayTimeHelper::registerQmlType();
    OauthClient::registerQmlType();
    CalendarModel::registerQmlType();
    DayHoursModel::registerQmlType();
    MonthListModel::registerQmlType();
    AbstractTimePeriodStorage::registerQmlType();
    ChunkProvider::registerQmlType();
    ColorTheme::registerQmlType();
    FontConfig::registerQmlType();
    GlobalTemporaries::registerQmlType();
    analytics::IconManager::registerQmlType();
    AbstractCameraButtonController::registerQmlType();
    CameraButton::registerQmlType();
    AccessHelper::registerQmlType();

    MediaPlayer::registerQmlTypes();

    qRegisterMetaType<MediaDewarpingParams>();
    qmlRegisterUncreatableType<MediaDewarpingParams>("nx.vms.client.core", 1, 0,
        "MediaDewarpingParams", "Cannot create an instance of MediaDewarpingParams.");

    qmlRegisterUncreatableType<QnResource>("Nx.Common", 1, 0, "Resource",
        "Cannot create an instance of Resource.");
    qmlRegisterUncreatableType<QnVirtualCameraResource>("Nx.Common", 1, 0, "VirtualCameraResource",
        "Cannot create an instance of VirtualCameraResource.");
    qmlRegisterUncreatableType<QnMediaServerResource>("Nx.Common", 1, 0, "MediaServerResource",
        "Cannot create an instance of MediaServerResource.");
    qmlRegisterUncreatableType<QnLayoutResource>("Nx.Common", 1, 0, "LayoutResource",
        "Cannot create an instance of LayoutResource.");
    qmlRegisterUncreatableType<QnVideoWallResource>("Nx.Common", 1, 0, "VideoWallResource",
        "Cannot create an instance of VideoWallResource.");
    qmlRegisterUncreatableMetaObject(welcome_screen::staticMetaObject,
        "nx.vms.client.core", 1, 0, "WelcomeScreen",
        "Cannot create an instance of WelcomeScreen.");

    qmlRegisterUncreatableMetaObject(nx::vms::api::dewarping::staticMetaObject, "nx.vms.api", 1, 0,
        "Dewarping", "Dewarping is a namespace");

    qmlRegisterUncreatableMetaObject(
        nx::vms::client::core::network::server_certificate::staticMetaObject,
        "nx.vms.client.core", 1, 0, "Certificate", "Certificate is a namespace");
    qRegisterMetaType<nx::vms::client::core::network::server_certificate::ValidationLevel>();

    TwoWayAudioController::registerQmlType();

    GridViewport::registerQmlType();

    qmlRegisterUncreatableMetaObject(nx::vms::api::staticMetaObject, "nx.vms.api", 1, 0,
        "API", "API is a namespace");
}

} // namespace nx::vms::client::core
