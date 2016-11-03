#include "client_core_meta_types.h"

#include <QtQml/QtQml>

#include <client_core/local_connection_data.h>
#include <test/qml_test_helper.h>
#include <ui/helpers/scene_position_listener.h>
#include <ui/video/video_output.h>
#include <client/forgotten_systems_manager.h>

#include <utils/common/app_info.h>

void QnClientCoreMetaTypes::initialize()
{
    qRegisterMetaType<QnLocalConnectionData>();
    qRegisterMetaTypeStreamOperators<QnLocalConnectionData>();
    qRegisterMetaType<QnLocalConnectionDataList>();
    qRegisterMetaTypeStreamOperators<QnLocalConnectionDataList>();

    qRegisterMetaType<QnStringSet>();
    qRegisterMetaTypeStreamOperators<QnStringSet>();

    qmlRegisterType<QnQmlTestHelper>("Nx.Test", 1, 0, "QmlTestHelper");
    qmlRegisterType<QnScenePositionListener>("com.networkoptix.qml", 1, 0, "QnScenePositionListener");
    qmlRegisterType<QnAppInfo>("com.networkoptix.qml", 1, 0, "QnAppInfo");
    qmlRegisterType<QnVideoOutput>("com.networkoptix.qml", 1, 0, "QnVideoOutput");
}
