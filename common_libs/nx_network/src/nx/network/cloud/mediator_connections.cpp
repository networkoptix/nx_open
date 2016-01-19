
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
//// BaseMediatorClient
////////////////////////////////////////////////////////////

BaseMediatorClient::BaseMediatorClient(std::shared_ptr<stun::AsyncClient> client)
:
    stun::AsyncClientUser(std::move(client))
{
}

template<typename RequestData, typename CompletionHandlerType>
void BaseMediatorClient::doRequest(
    nx::stun::cc::methods::Value method,
    RequestData requestData,
    CompletionHandlerType completionHandler)
{
    stun::Message request(
        stun::Header(
            stun::MessageClass::request,
            method));
    requestData.serialize(&request);

    sendRequestAndReceiveResponse(
        std::move(request),
        std::move(completionHandler));
}

template<typename ResponseData>
void BaseMediatorClient::sendRequestAndReceiveResponse(
    stun::Message request,
    std::function<void(nx::hpm::api::ResultCode, ResponseData)> completionHandler)
{
    const stun::cc::methods::Value method = 
        static_cast<stun::cc::methods::Value>(request.header.method);

    sendRequest(
        std::move(request),
        [this, method, /*std::move*/ completionHandler](    //TODO #ak #msvc2015 move to lambda
            SystemError::ErrorCode code,
            stun::Message message)
    {
        if (code != SystemError::noError)
        {
            NX_LOGX(lm("Error performing %1 request to connection_mediator. %2").
                arg(stun::cc::methods::toString(method)).
                arg(SystemError::toString(code)),
                cl_logDEBUG1);
            return completionHandler(api::ResultCode::networkError, ResponseData());
        }

        if (const auto error = stun::AsyncClient::hasError(code, message))
        {
            api::ResultCode resultCode = api::ResultCode::otherLogicError;
            if (const auto err = message.getAttribute<nx::stun::attrs::ErrorDescription>())
                resultCode = api::fromStunErrorToResultCode(*err);

            NX_LOGX(*error, cl_logDEBUG1);
            //TODO #ak get detailed error from response
            return completionHandler(resultCode, ResponseData());
        }

        ResponseData responseData;
        if (!responseData.parse(message))
        {
            NX_LOGX(lm("Failed to parse %1 response: %2").
                arg(stun::cc::methods::toString(method)).
                arg(responseData.errorText()), cl_logDEBUG1);
            return completionHandler(
                api::ResultCode::responseParseError,
                ResponseData());
        }

        completionHandler(
            api::ResultCode::ok,
            std::move(responseData));
    });
}

void BaseMediatorClient::sendRequestAndReceiveResponse(
    stun::Message request,
    std::function<void(nx::hpm::api::ResultCode)> completionHandler)
{
    const stun::cc::methods::Value method = 
        static_cast<stun::cc::methods::Value>(request.header.method);

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
            return completionHandler(api::ResultCode::networkError);
        }

        if (const auto error = stun::AsyncClient::hasError(code, message))
        {
            api::ResultCode resultCode = api::ResultCode::otherLogicError;
            if (const auto err = message.getAttribute< nx::stun::attrs::ErrorDescription >())
                resultCode = api::fromStunErrorToResultCode(*err);

            NX_LOGX(*error, cl_logDEBUG1);
            //TODO #ak get detailed error from response
            return completionHandler(resultCode);
        }

        completionHandler(api::ResultCode::ok);
    });
}


////////////////////////////////////////////////////////////
//// MediatorClientConnection
////////////////////////////////////////////////////////////

MediatorClientConnection::MediatorClientConnection(
    std::shared_ptr<stun::AsyncClient> client)
:
    BaseMediatorClient(std::move(client))
{
}

void MediatorClientConnection::resolve(
    api::ResolveRequest resolveData,
    std::function<void(
        api::ResultCode,
        api::ResolveResponse)> completionHandler)
{
    doRequest(
        stun::cc::methods::resolve,
        std::move(resolveData),
        std::move(completionHandler));
}

void MediatorClientConnection::connect(
    api::ConnectRequest connectData,
    std::function<void(
        api::ResultCode,
        api::ConnectResponse)> completionHandler)
{
    doRequest(
        stun::cc::methods::connect,
        std::move(connectData),
        std::move(completionHandler));
}

void MediatorClientConnection::connectionResult(
    nx::hpm::api::ConnectionResultRequest resultData,
    std::function<void(nx::hpm::api::ResultCode)> completionHandler)
{
    doRequest(
        stun::cc::methods::connectionResult,
        std::move(resultData),
        std::move(completionHandler));
}


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

void MediatorServerConnection::sendAuthRequest(
    stun::Message request,
    stun::AsyncClient::RequestHandler handler)
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
