
#include "mediator_connections.h"

#include <nx/utils/log/log.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/cloud/mediator_connector.h>


namespace nx {
namespace network {
namespace cloud {

typedef stun::cc::attrs::PublicEndpointList PublicEndpointList;

using namespace nx::hpm;


////////////////////////////////////////////////////////////
//// MediatorServerConnection
////////////////////////////////////////////////////////////

MediatorServerConnection::MediatorServerConnection(
    std::shared_ptr<stun::AsyncClient> client,
    MediatorConnector* connector)
:
    BaseMediatorClient(std::move(client)),
    m_connector(connector)
{
    // TODO subscribe for indications
}

void MediatorServerConnection::ping(
    nx::hpm::api::PingRequest requestData,
    std::function<void(
        nx::hpm::api::ResultCode,
        nx::hpm::api::PingResponse)> completionHandler)
{
    doAuthRequest(
        stun::cc::methods::ping,
        std::move(requestData),
        std::move(completionHandler));
}

void MediatorServerConnection::bind(
    nx::hpm::api::BindRequest requestData,
    std::function<void(api::ResultCode)> completionHandler)
{
    doAuthRequest(
        stun::cc::methods::bind,
        std::move(requestData),
        std::move(completionHandler));
}

void MediatorServerConnection::listen(
    nx::hpm::api::ListenRequest listenParams,
    std::function<void(nx::hpm::api::ResultCode)> completionHandler)
{
    doAuthRequest(
        stun::cc::methods::listen,
        std::move(listenParams),
        std::move(completionHandler));
}

void MediatorServerConnection::connectionAck(
    nx::hpm::api::ConnectionAckRequest request,
    std::function<void(nx::hpm::api::ResultCode)> completionHandler)
{
    doAuthRequest(
        stun::cc::methods::connectionAck,
        std::move(request),
        std::move(completionHandler));
}

void MediatorServerConnection::setOnConnectionRequestedHandler(
    std::function<void(nx::hpm::api::ConnectionRequestedEvent)> /*handler*/)
{
    //TODO #ak
}

template<typename RequestData, typename CompletionHandlerType>
void MediatorServerConnection::doAuthRequest(
    nx::stun::cc::methods::Value method,
    RequestData requestData,
    CompletionHandlerType completionHandler)
{
    stun::Message request(
        stun::Header(
            stun::MessageClass::request,
            method));
    requestData.serialize(&request);

    if (auto credentials = m_connector->getSystemCredentials())
    {
        request.newAttribute<stun::cc::attrs::SystemId>(credentials->systemId);
        request.newAttribute<stun::cc::attrs::ServerId>(credentials->serverId);
        request.insertIntegrity(credentials->systemId, credentials->key);
    }

    sendRequestAndReceiveResponse(
        std::move(request),
        std::move(completionHandler));
}

} // namespace cloud
} // namespace network
} // namespace nx
