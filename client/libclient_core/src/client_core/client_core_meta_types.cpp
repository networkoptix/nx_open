#include "client_core_meta_types.h"

#include <QtQml/QtQml>

#include <client_core/user_recent_connection_data.h>
#include <test/qml_test_helper.h>

void QnClientCoreMetaTypes::initialize()
{
    qRegisterMetaType<QnUserRecentConnectionData>();
    qRegisterMetaTypeStreamOperators<QnUserRecentConnectionData>();
    qRegisterMetaType<QnUserRecentConnectionDataList>();
    qRegisterMetaTypeStreamOperators<QnUserRecentConnectionDataList>();

    qmlRegisterType<QnQmlTestHelper>("Nx.Test", 1, 0, "QmlTestHelper");
}
