// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "router.h"

#include <common/static_common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/abstract_ec_connection.h>

QString QnRoute::toString() const
{
    return nx::format("%1 (%2 %3)").args(
        id,
        gatewayId.isNull() ? QString("direct") : (
            gatewayId.toString() + "+" + QString::number(distance)),
        reverseConnect ? "reverse" : addr.toString());
}

QnRouter::QnRouter(nx::vms::discovery::Manager* moduleManager, QObject* parent):
    QObject(parent),
    m_moduleManager(moduleManager)
{
}

QnRoute QnRouter::routeTo(const QnUuid& serverId, nx::vms::common::SystemContext* context)
{
    if (!NX_ASSERT(context))
        return {};

    auto server = context->resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    return routeTo(server);
}

QnRoute QnRouter::routeTo(const QnMediaServerResourcePtr& server)
{
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return {}; //< Do not route to unknown or offline server.

    QnRoute result;
    result.id = server->getId();

    // Route to itself (mediaserver-side only).
    if (server->getId() == server->systemContext()->peerId())
    {
        result.addr = nx::network::url::getEndpoint(server->getApiUrl());
        return result;
    }

    // Check if we have direct access to peer.
    if (const auto endpoint = m_moduleManager->getEndpoint(result.id))
    {
        result.addr = *endpoint;
        return result;
    }

    auto connection = server->systemContext()->ec2Connection();
    if (!connection)
        return result; //< No connection to the peer network, can't route.

    if (nx::vms::api::PeerData::isClient(qnStaticCommon->localPeerType()))
    {
        // Proxy via current server to the other servers (client side only)
        result.gatewayId = connection->moduleInformation().id;

        if (const auto endpoint = m_moduleManager->getEndpoint(result.gatewayId))
            result.addr = *endpoint;
        else
            NX_WARNING(this, "No primary interface found for current server yet.");

        return result;
    }

    result.distance = INT_MAX;
    QnUuid routeVia = connection->routeToPeerVia(result.id, &result.distance, &result.addr);
    if (!result.addr.isNull())
        return result; //< Peer has found in message bus among outgoing connections.
    if (routeVia.isNull())
        return result; // can't route

    if (routeVia == result.id)
    {
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
