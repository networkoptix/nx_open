
#include "mediator_connections.h"

#include <nx/utils/log/log.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/cloud/mediator_connector.h>

namespace nx {
namespace network {
namespace cloud {

typedef stun::cc::attrs::PublicEndpointList PublicEndpointList;

////////////////////////////////////////////////////////////
//// MediatorClientConnection
////////////////////////////////////////////////////////////

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

    request.newAttribute<stun::cc::attrs::PeerId>("SomeClientId"); // TODO
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

void MediatorClientConnection::resolve(
    api::ResolveRequest resolveData,
    std::function<void(api::ResultCode, api::ResolveResponse)> handler)
{
    doRequest(
        stun::cc::methods::resolve,
        std::move(resolveData),
        std::move(handler));
}

template<typename RequestData, typename ResponseData>
void MediatorClientConnection::doRequest(
    nx::stun::cc::methods::Value method,
    RequestData requestData,
    std::function<void(api::ResultCode, ResponseData)> completionHandler)
{
    stun::Message request(stun::Header(
        stun::MessageClass::request,
        method));
    requestData.serialize(&request);

    sendRequest(
        std::move(request),
        [this, method, /*std::move*/ completionHandler](
            SystemError::ErrorCode code,
            stun::Message message)
        {
            if (code != SystemError::noError)
            {
                NX_LOGX(lm("Error performing %1 request to connection_mediator. %2").
                    arg(stun::cc::methods::toString(method)).arg(SystemError::toString(code)),
                    cl_logDEBUG1);
                return completionHandler(api::ResultCode::networkError, ResponseData());
            }

            ResponseData responseData;
            if (const auto error = stun::AsyncClient::hasError(code, message))
            {
                api::ResultCode resultCode = api::ResultCode::otherLogicError;
                if (const auto err = message.getAttribute< nx::stun::attrs::ErrorDescription >())
                    resultCode = api::fromStunErrorToResultCode(*err);

                NX_LOGX(*error, cl_logDEBUG1);
                //TODO #ak get detailed error from response
                return completionHandler(resultCode, ResponseData());
            }

            if (!responseData.parse(message))
            {
                NX_LOGX(lm("Failed to parse %1 response: %2").
                    arg(stun::cc::methods::toString(method)).
                    arg(responseData.errorText()), cl_logDEBUG1);
                return completionHandler(api::ResultCode::responseParseError, ResponseData());
            }

            completionHandler(api::ResultCode::ok, std::move(responseData));
        });
}


////////////////////////////////////////////////////////////
//// MediatorSystemConnection
////////////////////////////////////////////////////////////

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
        std::function<void(api::ResultCode, bool)> handler)
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
            return handler(api::ResultCode::otherLogicError, false);
        }

        handler(api::ResultCode::ok, true);
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

} // namespace cloud
} // namespace network
} // namespace nx
