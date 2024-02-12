// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "router.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/abstract_ec_connection.h>

using namespace nx::vms::common;

namespace {

QnRoute routeTo(
    SystemContext* systemContext,
    const nx::Uuid& serverId,
    const QnMediaServerResourcePtr& server)
{
    if (!server)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "Server is not found. Can not route");
        return {}; //< Do not route to unknown Server.
    }
    if (server->getStatus() != nx::vms::api::ResourceStatus::online)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "Server %1 is offline. Can not route", server->getId());
        return {}; //< Do not route to offline Server.
    }

    if (!NX_ASSERT(systemContext) || !systemContext->moduleDiscoveryManager())
    {
        NX_VERBOSE(NX_SCOPE_TAG, "Routing is not enabled in the Context.");
        return {}; // Routing is not enabled in the Context.
    }

    auto moduleDiscoveryManager = systemContext->moduleDiscoveryManager();

    QnRoute result;
    result.id = serverId;

    // Route to itself (mediaserver-side only).
    if (serverId == systemContext->peerId())
    {
        result.addr =
            nx::network::SocketAddress(nx::network::HostAddress::localhost, server->getPort());
        return result;
    }

    // Check if we have direct access to peer.
    if (const auto endpoint = moduleDiscoveryManager->getEndpoint(result.id))
    {
        result.addr = *endpoint;
        return result;
    }

    auto connection = systemContext->messageBusConnection();
    if (!connection)
        return result; //< No connection to the peer network, can't route.

    if (nx::vms::api::PeerData::isClient(appContext()->localPeerType()))
    {
        // Proxy via current server to the other servers (client side only)
        result.gatewayId = connection->moduleInformation().id;

        if (const auto endpoint = moduleDiscoveryManager->getEndpoint(result.gatewayId))
            result.addr = *endpoint;
        else
            NX_WARNING(NX_SCOPE_TAG, "No primary interface found for current server yet.");

        return result;
    }

    result.distance = INT_MAX;
    nx::Uuid routeVia = connection->routeToPeerVia(result.id, &result.distance, &result.addr);
    if (!result.addr.isNull())
        return result; //< Peer has found in message bus among outgoing connections.
    if (routeVia.isNull())
        return result; // can't route

    if (routeVia == result.id)
    {
        // peer accessible directly, but no address available (bc of NAT),
        // so we need backwards connection
        result.reverseConnect = true;
        return result;
    }

    // route gateway is found
    result.gatewayId = routeVia;
    if (const auto endpoint = moduleDiscoveryManager->getEndpoint(result.gatewayId))
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

} // namespace

QString QnRoute::toString() const
{
    return nx::format("%1 (%2 %3)").args(
        id,
        gatewayId.isNull() ? QString("direct") : (
            gatewayId.toString() + "+" + QString::number(distance)),
        reverseConnect ? "reverse" : addr.toString());
}

QnRoute QnRouter::routeTo(const nx::Uuid& serverId, SystemContext* context)
{
    if (!NX_ASSERT(context))
        return {};

    auto server = context->resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    return ::routeTo(context, serverId, server);
}

QnRoute QnRouter::routeTo(const QnMediaServerResourcePtr& server)
{
    if (!NX_ASSERT(server))
        return {};

    return ::routeTo(server->systemContext(), server->getId(), server);
}
