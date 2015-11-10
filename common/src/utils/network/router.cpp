#include "router.h"

#include <QtCore/QElapsedTimer>

// #include "utils/common/log.h"
// #include "nx_ec/dummy_handler.h"
// #include "nx_ec/ec_api.h"

#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/network/module_finder.h>

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

    bool isknownServer = qnResPool->getResourceById<QnMediaServerResource>(id) != 0;
    bool isClient = qnCommon->remoteGUID() != qnCommon->moduleGUID();
    if (!isknownServer && isClient) {
		if (qnCommon->remoteGUID().isNull())
            return result;
			
        result.gatewayId = qnCommon->remoteGUID(); // proxy via current server to the other/incompatible system (client side only)
        result.addr = m_moduleFinder->primaryAddress(result.gatewayId);
        Q_ASSERT_X(!result.addr.isNull(), Q_FUNC_INFO, "QnRouter: no primary interface found for current EC.");
		// todo: add distance for camera route
        return result;
    }

    result.distance = INT_MAX;
    QnUuid routeVia = connection->routeToPeerVia(id, &result.distance);
    if (routeVia == id || routeVia.isNull())
        return result; // can't route

        // peer accesible directly, but no address avaliable (bc of NAT),
        // so we need backwards connection
    // route gateway is found
    result.gatewayId = routeVia;
    result.addr = m_moduleFinder->primaryAddress(routeVia);
    if (result.addr.isNull())
        result.reverseConnect = true;
    return result;
}

void QnRouter::updateRequest(QUrl& url, nx_http::HttpHeaders& headers, const QnUuid &id)
{
    QnRoute route = routeTo(id);
    if (route.isValid()) 
    {
        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
        headers.emplace("x-server-guid", id.toByteArray());
    }
}
