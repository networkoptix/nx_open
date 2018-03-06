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
#include <nx/client/core/animation/kinetic_animation.h>
#include <nx/client/core/ui/frame_section.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/core/utils/quick_item_mouse_tracker.h>
#include <nx/client/core/two_way_audio/two_way_audio_mode_controller.h>

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

    qmlRegisterType<QnQmlTestHelper>("Nx.Test", 1, 0, "QmlTestHelper");
    qmlRegisterType<QnScenePositionListener>("com.networkoptix.qml", 1, 0, "QnScenePositionListener");
    qmlRegisterType<QnAppInfo>("com.networkoptix.qml", 1, 0, "QnAppInfo");
    qmlRegisterType<QnVideoOutput>("Nx.Media", 1, 0, "VideoOutput");

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
    qmlRegisterUncreatableType<QnSoftwareVersion>(
        "Nx", 1, 0, "SoftwareVersion", QLatin1String("Cannot create an instance of SoftwareVersion."));

    FrameSection::registedQmlType();
    Geometry::registerQmlType();
    QuickItemMouseTracker::registerQmlType();

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

    nx::client::core::TwoWayAudioController::registerQmlType();
}

} // namespace core
} // namespace client
} // namespace nx
