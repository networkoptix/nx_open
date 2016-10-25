#include "client_core_meta_types.h"

#include <QtQml/QtQml>

#include <client_core/local_connection_data.h>
#include <test/qml_test_helper.h>
#include <ui/helpers/scene_position_listener.h>

#include <utils/common/app_info.h>

void QnClientCoreMetaTypes::initialize()
{
    qRegisterMetaType<QnLocalConnectionData>();
    qRegisterMetaType<QnLocalConnectionDataList>();

    qmlRegisterType<QnQmlTestHelper>("Nx.Test", 1, 0, "QmlTestHelper");
    qmlRegisterType<QnScenePositionListener>("com.networkoptix.qml", 1, 0, "QnScenePositionListener");
    qmlRegisterType<QnAppInfo>("com.networkoptix.qml", 1, 0, "QnAppInfo");
}
