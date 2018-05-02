#include "router.h"

// #include <nx/utils/log/log.h>
// #include "nx_ec/dummy_handler.h"
// #include "nx_ec/ec_api.h"

#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include "nx/vms/discovery/manager.h"
#include <common/common_module.h>

QnRouter::QnRouter(
    QObject* parent,
    nx::vms::discovery::Manager* moduleManager)
:
    QObject(parent),
    QnCommonModuleAware(parent),
    m_moduleManager(moduleManager)
{
}

QnRouter::~QnRouter() {}

QnRoute QnRouter::routeTo(const QnUuid &id)
{
    QnRoute result;
    result.id = id;
    if (const auto endpoint = m_moduleManager->getEndpoint(id))
    {
        result.addr = *endpoint;
        return result; // direct access to peer
    }

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return result; // no connection to the server, can't route

    bool isknownServer = commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(id) != 0;
    bool isClient = commonModule()->remoteGUID() != commonModule()->moduleGUID();
    if (!isknownServer && isClient) {
		if (commonModule()->remoteGUID().isNull())
            return result;

        result.gatewayId = commonModule()->remoteGUID(); // proxy via current server to the other/incompatible system (client side only)
        if (const auto endpoint = m_moduleManager->getEndpoint(result.gatewayId))
            result.addr = *endpoint;
        else
            NX_ASSERT(false, "No primary interface found for current EC.");

        // todo: add distance for camera route
        return result;
    }

    result.distance = INT_MAX;
    QnUuid routeVia = connection->routeToPeerVia(id, &result.distance);
    if (routeVia.isNull())
        return result; // can't route

    if (routeVia == id) {
        // peer accesible directly, but no address avaliable (bc of NAT),
        // so we need backwards connection
        result.reverseConnect = true;
        return result;
    }

    // route gateway is found
    result.gatewayId = routeVia;
    if (const auto endpoint = m_moduleManager->getEndpoint(result.gatewayId))
        result.addr = *endpoint;
    else
        result.reverseConnect = true;

    return result;
}

void QnRouter::updateRequest(QUrl& url, nx::network::http::HttpHeaders& headers, const QnUuid &id)
{
    QnRoute route = routeTo(id);
    if (route.isValid())
    {
        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
        headers.emplace("x-server-guid", id.toByteArray());
    }
}
