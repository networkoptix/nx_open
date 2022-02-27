// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "router.h"

#include <nx/network/url/url_parse_helper.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/abstract_ec_connection.h>

#include "nx/vms/discovery/manager.h"
#include <common/common_module.h>

QString QnRoute::toString() const
{
    return nx::format("%1 (%2 %3)").args(
        id,
        gatewayId.isNull() ? QString("direct") : (
            gatewayId.toString() + "+" + QString::number(distance)),
        reverseConnect ? "reverse" : addr.toString());
}

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

    const auto server = commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(id);
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return result; //< Do not route to unknown or offline server.

    if (server->getId() == commonModule()->moduleGUID())
    {
        result.addr = nx::network::url::getEndpoint(server->getApiUrl());
        return result;
    }

    if (const auto endpoint = m_moduleManager->getEndpoint(id))
    {
        result.addr = *endpoint;
        return result; // direct access to peer
    }

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return result; // no connection to the server, can't route

    bool isClient = !commonModule()->remoteGUID().isNull()
        && commonModule()->remoteGUID() != commonModule()->moduleGUID();
    if (isClient)
    {
        result.gatewayId = commonModule()->remoteGUID(); // proxy via current server to the other servers (client side only)
        if (const auto endpoint = m_moduleManager->getEndpoint(result.gatewayId))
            result.addr = *endpoint;
        else
            NX_WARNING(this, "No primary interface found for current EC yet.");

        return result;
    }

    result.distance = INT_MAX;
    QnUuid routeVia = connection->routeToPeerVia(id, &result.distance, &result.addr);
    if (!result.addr.isNull())
        return result; //< Peer has found in message bus among outgoing connections.
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
    {
        result.addr = *endpoint;
    }
    else
    {
        int gatewayDistance = 0;
        connection->routeToPeerVia(result.gatewayId, &gatewayDistance, &result.addr);
        if (result.addr.isNull())
            result.reverseConnect = true;
    }

    return result;
}
