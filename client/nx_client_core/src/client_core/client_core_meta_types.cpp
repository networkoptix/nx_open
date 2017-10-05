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

static QObject* createNxGlobals(QQmlEngine*, QJSEngine*)
{
    return new NxGlobalsObject();
}

void QnClientCoreMetaTypes::initialize()
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

    qmlRegisterType<nx::client::core::ui::positioners::Grid>("Nx.Positioners", 1, 0, "Grid");

    qmlRegisterType<nx::client::core::animation::KineticAnimation>(
        "Nx.Animations", 1, 0, "KineticAnimation");

    qmlRegisterSingletonType<NxGlobalsObject>("Nx", 1, 0, "Nx", &createNxGlobals);
    qmlRegisterUncreatableType<QnUuid>(
        "Nx.Utils", 1, 0, "Uuid", QLatin1String("Cannot create an instance of Uuid."));
    qRegisterMetaType<QnUrlHelper>();
    qmlRegisterUncreatableType<QnUrlHelper>(
        "Nx", 1, 0, "UrlHelper", QLatin1String("Cannot create an instance of UrlHelper."));
    qmlRegisterUncreatableType<QnSoftwareVersion>(
        "Nx", 1, 0, "SoftwareVersion", QLatin1String("Cannot create an instance of SoftwareVersion."));

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
