#include "mediator_connections.h"

#include <nx/utils/log/log.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/cloud_connectivity/mediator_connector.h>

namespace nx {
namespace cc {

typedef stun::cc::attrs::PublicEndpointList PublicEndpointList;

MediatorClientConnection::MediatorClientConnection(
        std::shared_ptr<stun::AsyncClient> client)
    : stun::AsyncClientUser(std::move(client))
{
}

void MediatorClientConnection::connect(
        String host, std::function<void(std::list<SocketAddress>)> handler)
{
    stun::Message request(stun::Header(stun::MessageClass::request,
                                       stun::cc::methods::connect));

    request.newAttribute<stun::cc::attrs::ClientId>("SomeClientId"); // TODO
    request.newAttribute<stun::cc::attrs::HostName>(host);
    sendRequest(std::move(request),
                [=](SystemError::ErrorCode code, stun::Message message)
    {
        if(const auto error = stun::AsyncClient::hasError(code, message))
        {
            NX_LOGX(*error, cl_logDEBUG1);
            return handler(std::list<SocketAddress>());
        }

        std::list< SocketAddress > endpoints;
        if(auto eps = message.getAttribute<PublicEndpointList>())
            endpoints = eps->get();

        handler(std::move(endpoints));
    });
}

MediatorSystemConnection::MediatorSystemConnection(
        std::shared_ptr<stun::AsyncClient> client,
        MediatorConnector* connector)
    : stun::AsyncClientUser(std::move(client))
    , m_connector(connector)
{
    // TODO subscribe for indications
}

void MediatorSystemConnection::ping(
        std::list<SocketAddress> addresses,
        std::function<void(bool, std::list<SocketAddress>)> handler)
{
    stun::Message request(stun::Header(stun::MessageClass::request,
                                       stun::cc::methods::ping));

    request.newAttribute<PublicEndpointList>(addresses);
    sendAuthRequest(std::move(request),
                [=](SystemError::ErrorCode code, stun::Message message)
    {
        if(const auto error = stun::AsyncClient::hasError(code, message))
        {
            NX_LOGX(*error, cl_logDEBUG1);
            return handler(false, std::list<SocketAddress>());
        }

        std::list< SocketAddress > endpoints;
        if(auto eps = message.getAttribute<PublicEndpointList>())
            endpoints = eps->get();

        handler(true, std::move(endpoints));
    });
}

void MediatorSystemConnection::bind(
        std::list<SocketAddress> addresses,
        std::function<void(bool)> handler)
{
    stun::Message request(stun::Header(stun::MessageClass::request,
                                       stun::cc::methods::bind));

    request.newAttribute<PublicEndpointList>(addresses);
    sendAuthRequest(std::move(request),
                [=](SystemError::ErrorCode code, stun::Message message)
    {
        if(const auto error = stun::AsyncClient::hasError(code, message))
        {
            NX_LOGX(*error, cl_logDEBUG1);
            return handler(false);
        }

        handler(true);
    });
}

void MediatorSystemConnection::sendAuthRequest(
        stun::Message request, stun::AsyncClient::RequestHandler handler)
{
    if (auto credentials = m_connector->getSystemCredentials())
    {
        request.newAttribute<stun::cc::attrs::SystemId>(credentials->systemId);
        request.newAttribute<stun::cc::attrs::ServerId>(credentials->serverId);
        request.insertIntegrity(credentials->systemId, credentials->key);

        sendRequest(std::move(request), std::move(handler));
    }
    else
    {
        // can not send anything while credentials are not known
        handler(SystemError::notConnected, stun::Message());
    }
}

} // namespase cc
} // namespase nx
