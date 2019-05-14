#include "client_core_meta_types.h"

#include <atomic>

#include <QtQml/QtQml>

#include <core/ptz/media_dewarping_params.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <test/qml_test_helper.h>
#include <ui/helpers/scene_position_listener.h>
#include <ui/video/video_output.h>
#include <ui/models/authentication_data_model.h>
#include <ui/models/system_hosts_model.h>
#include <ui/models/ordered_systems_model.h>
#include <ui/models/model_data_accessor.h>
#include <ui/positioners/grid_positioner.h>
#include <client/forgotten_systems_manager.h>
#include <utils/common/app_info.h>
#include <helpers/nx_globals_object.h>
#include <common/common_meta_types.h>

#include <nx/client/core/animation/kinetic_animation.h>
#include <nx/vms/client/core/common/utils/encoded_credentials.h>
#include <nx/client/core/media/media_player.h>
#include <nx/client/core/resource/resource_helper.h>
#include <nx/client/core/resource/media_resource_helper.h>
#include <nx/client/core/motion/helpers/media_player_motion_provider.h>
#include <nx/client/core/motion/helpers/camera_motion_helper.h>
#include <nx/client/core/motion/items/motion_mask_item.h>
#include <nx/client/core/ui/frame_section.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/core/utils/quick_item_mouse_tracker.h>
#include <nx/client/core/utils/operation_manager.h>
#include <nx/client/core/two_way_audio/two_way_audio_mode_controller.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/api/data/system_information.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::core {

namespace {

static QObject* createNxGlobals(QQmlEngine*, QJSEngine*)
{
    return new nx::vms::client::core::NxGlobalsObject();
}

static QObject* createAppInfo(QQmlEngine*, QJSEngine*)
{
    return new QnAppInfo();
}

} // namespace

void initializeMetaTypes()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    QnCommonMetaTypes::initialize();

    QnJsonSerializer::registerSerializer<EncodedCredentials>();

    // This type is used to store some values in settings as QVariant, so we need to maintain the
    // alias at it was used long time ago.
    // TODO: #GDM Create a settings migration from QVariant to json and get rid of these operators.
    qRegisterMetaType<QnStringSet>("QnStringSet");
    qRegisterMetaType<QSet<QString>>();

    qRegisterMetaTypeStreamOperators<QnStringSet>();

    qRegisterMetaType<nx::media::PlayerStatistics>();

    qmlRegisterType<QnQmlTestHelper>("Nx.Test", 1, 0, "QmlTestHelper");
    qmlRegisterType<QnScenePositionListener>("com.networkoptix.qml", 1, 0, "QnScenePositionListener");
    qmlRegisterType<QnAppInfo>("com.networkoptix.qml", 1, 0, "QnAppInfo");
    qmlRegisterType<QnVideoOutput>("Nx.Media", 1, 0, "VideoOutput");
    qmlRegisterType<ResourceHelper>("Nx.Core", 1, 0, "ResourceHelper");
    qmlRegisterType<MediaResourceHelper>("Nx.Core", 1, 0, "MediaResourceHelper");

    qmlRegisterType<AuthenticationDataModel>("Nx.Models", 1, 0, "AuthenticationDataModel");
    qmlRegisterType<QnSystemHostsModel>("Nx.Models", 1, 0, "SystemHostsModel");
    qmlRegisterType<QnOrderedSystemsModel>("Nx.Models", 1, 0, "OrderedSystemsModel");
    qmlRegisterType<nx::client::ModelDataAccessor>("Nx.Models", 1, 0, "ModelDataAccessor");

    qmlRegisterType<ui::positioners::Grid>("Nx.Positioners", 1, 0, "Grid");

    qmlRegisterType<animation::KineticAnimation>(
        "Nx.Animations", 1, 0, "KineticAnimation");

    qmlRegisterSingletonType<QnAppInfo>("Nx", 1, 0, "AppInfo", &createAppInfo);

    qmlRegisterSingletonType<NxGlobalsObject>("Nx", 1, 0, "NxGlobals", &createNxGlobals);
    qmlRegisterUncreatableType<QnUuid>(
        "Nx.Utils", 1, 0, "Uuid", QLatin1String("Cannot create an instance of Uuid."));
    qmlRegisterUncreatableType<utils::Url>(
        "Nx.Utils", 1, 0, "Url", QLatin1String("Cannot create an instance of Url."));
    qmlRegisterUncreatableType<nx::vms::api::SoftwareVersion>(
        "Nx", 1, 0, "SoftwareVersion", QLatin1String("Cannot create an instance of SoftwareVersion."));
    qmlRegisterUncreatableType<nx::vms::api::SystemInformation>(
        "Nx", 1, 0, "SystemInformation", QLatin1String("Cannot create an instance of SystemInformation."));

    FrameSection::registedQmlType();
    Geometry::registerQmlType();
    QuickItemMouseTracker::registerQmlType();
    CameraMotionHelper::registerQmlType();
    MediaPlayerMotionProvider::registerQmlType();
    MotionMaskItem::registerQmlType();

    /* NxMediaPlayer should not be used.
    It is here only to allow assignments of MediaPlayer to properties of this type. */
    qmlRegisterType<MediaPlayer>("Nx.Media", 1, 0, "MediaPlayer");
    qmlRegisterUncreatableType<nx::media::Player>("Nx.Media", 1, 0, "NxMediaPlayer", lit("Cannot create an instance of abstract class."));

    qmlRegisterUncreatableType<QnMediaDewarpingParams>("Nx.Media", 1, 0, "MediaDewarpingParams",
        QLatin1String("Cannot create an instance of QnMediaDewarpingParams."));

    qmlRegisterUncreatableType<QnResource>("Nx.Common", 1, 0, "Resource",
        QLatin1String("Cannot create an instance of Resource."));
    qmlRegisterUncreatableType<QnVirtualCameraResource>("Nx.Common", 1, 0, "VirtualCameraResource",
        QLatin1String("Cannot create an instance of VirtualCameraResource."));
    qmlRegisterUncreatableType<QnMediaServerResource>("Nx.Common", 1, 0, "MediaServerResource",
        QLatin1String("Cannot create an instance of MediaServerResource."));
    qmlRegisterUncreatableType<QnLayoutResource>("Nx.Common", 1, 0, "LayoutResource",
        QLatin1String("Cannot create an instance of LayoutResource."));

    nx::vms::client::core::TwoWayAudioController::registerQmlType();
    nx::vms::client::core::OperationManager::registerQmlType();
}

} // namespace nx::vms::client::core
