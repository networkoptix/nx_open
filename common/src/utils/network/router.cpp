#include "router.h"

#include <QtCore/QElapsedTimer>

#include "utils/common/log.h"
#include "nx_ec/dummy_handler.h"
#include "nx_ec/ec_api.h"
#include "common/common_module.h"
#include "module_finder.h"
#include "api/app_server_connection.h"

namespace {
    static const int runtimeDataUpdateTimeout = 3000;
} // anonymous namespace

QnRouter::QnRouter(QnModuleFinder *moduleFinder, QObject *parent) :
    m_moduleFinder(moduleFinder),
    QObject(parent)
{
}

QnRouter::~QnRouter() {}

QnRoute QnRouter::routeTo(const QnUuid &id)
{
    QnRoute result;
    result.id = id;
    result.addr = m_moduleFinder->preferredModuleAddress(id);
    if (!result.addr.isNull())
        return result; // direct access to peer

    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return result; // no connection to the server, can't route

    QnUuid routeVia = connection->routeToPeerVia(id);

    // todo: add code for other systems accessible from the current server (for client side only)
    if (0) {
        result.gatewayId = qnCommon->remoteGUID(); // proxy via current server to the other/incompatible system (client side only)
        result.addr = m_moduleFinder->preferredModuleAddress(result.gatewayId);
        return result;
    }

    if (routeVia == id || routeVia.isNull())
        return result; // can't route
    result.addr = m_moduleFinder->preferredModuleAddress(routeVia);
    if (!result.addr.isNull())
        result.gatewayId = routeVia; // route gateway is found
    return result;
}
