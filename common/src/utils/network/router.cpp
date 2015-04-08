#include "router.h"

#include <QtCore/QElapsedTimer>

#include "utils/common/log.h"
#include "nx_ec/dummy_handler.h"
#include "nx_ec/ec_api.h"
#include "common/common_module.h"
#include "module_finder.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"

QnRouter::QnRouter(QnModuleFinder *moduleFinder, QObject *parent) :
    QObject(parent),
    m_moduleFinder(moduleFinder)
{
}

QnRouter::~QnRouter() {}

QnRoute QnRouter::routeTo(const QnUuid &id)
{
    QnRoute result;
    result.id = id;
    result.addr = m_moduleFinder->primaryAddress(id);
    if (!result.addr.isNull())
        return result; // direct access to peer

    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return result; // no connection to the server, can't route

    bool isknownServer = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>() != 0;
    bool isClient = qnCommon->remoteGUID() != qnCommon->moduleGUID();
    if (!isknownServer && isClient) {
        result.gatewayId = qnCommon->remoteGUID(); // proxy via current server to the other/incompatible system (client side only)
        result.addr = m_moduleFinder->primaryAddress(result.gatewayId);
        Q_ASSERT_X(!result.addr.isNull(), Q_FUNC_INFO, "QnRouter: no primary interface found for current EC.");
        return result;
    }

    QnUuid routeVia = connection->routeToPeerVia(id);
    if (routeVia == id || routeVia.isNull())
        return result; // can't route
    result.addr = m_moduleFinder->primaryAddress(routeVia);
    if (!result.addr.isNull())
        result.gatewayId = routeVia; // route gateway is found
    return result;
}
