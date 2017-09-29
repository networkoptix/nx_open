#include "client_core_meta_types.h"

#include <QtQml/QtQml>

#include <core/ptz/media_dewarping_params.h>
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

    qmlRegisterUncreatableType<NxGlobalsObject>(
        "Nx", 1, 0, "NxGlobals", QLatin1String("Cannot create an instance of NxGlobals."));
    qmlRegisterUncreatableType<QnUrlHelper>(
        "Nx.Utils", 1, 0, "Uuid", QLatin1String("Cannot create an instance of Uuid."));
    qRegisterMetaType<QnUrlHelper>();
    qmlRegisterUncreatableType<QnUrlHelper>(
        "Nx", 1, 0, "UrlHelper", QLatin1String("Cannot create an instance of UrlHelper."));
    qmlRegisterUncreatableType<QnSoftwareVersion>(
        "Nx", 1, 0, "SoftwareVersion", QLatin1String("Cannot create an instance of SoftwareVersion."));

    qmlRegisterUncreatableType<QnMediaDewarpingParams>("Nx.Media", 1, 0, "MediaDewarpingParams",
        QLatin1String("Cannot create an instance of QnMediaDewarpingParams."));
}
