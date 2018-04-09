#include "client_core_meta_types.h"

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
#include <helpers/url_helper.h>

#include <nx/client/core/animation/kinetic_animation.h>
#include <nx/client/core/media/media_player.h>
#include <nx/client/core/resource/resource_helper.h>
#include <nx/client/core/resource/media_resource_helper.h>
#include <nx/client/core/ui/frame_section.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/core/utils/quick_item_mouse_tracker.h>

namespace nx {
namespace client {
namespace core {

namespace {

static QObject* createNxGlobals(QQmlEngine*, QJSEngine*)
{
    return new nx::client::core::NxGlobalsObject();
}

static QObject* createAppInfo(QQmlEngine*, QJSEngine*)
{
    return new QnAppInfo();
}

} // namespace

void initializeMetaTypes()
{
    qRegisterMetaType<QnStringSet>();
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
    qRegisterMetaType<QnUrlHelper>();
    qmlRegisterUncreatableType<QnUrlHelper>(
        "Nx", 1, 0, "UrlHelper", QLatin1String("Cannot create an instance of UrlHelper."));
    qmlRegisterUncreatableType<QnSoftwareVersion>(
        "Nx", 1, 0, "SoftwareVersion", QLatin1String("Cannot create an instance of SoftwareVersion."));

    FrameSection::registedQmlType();
    Geometry::registerQmlType();
    QuickItemMouseTracker::registerQmlType();

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
}

} // namespace core
} // namespace client
} // namespace nx
