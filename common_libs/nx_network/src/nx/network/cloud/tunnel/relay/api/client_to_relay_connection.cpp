#include "client_to_relay_connection.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/std/cpp14.h>
#include <http/custom_headers.h>

#include "relay_api_http_paths.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {

//-------------------------------------------------------------------------------------------------
// ClientFactory

static ClientFactory::CustomFactoryFunc factoryFunc;

std::unique_ptr<Client> ClientFactory::create(
    const QUrl& baseUrl)
{
    if (factoryFunc)
        return factoryFunc(baseUrl);
    return std::make_unique<ClientImpl>(baseUrl);
}

ClientFactory::CustomFactoryFunc 
    ClientFactory::setCustomFactoryFunc(CustomFactoryFunc newFactoryFunc)
{
    CustomFactoryFunc oldFunc;
    oldFunc.swap(factoryFunc);
    factoryFunc = std::move(newFactoryFunc);
    return oldFunc;
}

//-------------------------------------------------------------------------------------------------
// ClientImpl

ClientImpl::ClientImpl(const QUrl& baseUrl):
    m_baseUrl(baseUrl),
    m_prevSysErrorCode(SystemError::noError)
{
}

void ClientImpl::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (const auto& request: m_activeRequests)
        request->bindToAioThread(aioThread);
}

void ClientImpl::beginListening(
    const nx::String& peerName,
    BeginListeningHandler completionHandler)
{
    using namespace std::placeholders;

    BeginListeningRequest request;
    request.peerName = peerName.toStdString();

    issueRequest<BeginListeningRequest, BeginListeningResponse>(
        std::move(request),
        kServerIncomingConnectionsPath,
        {peerName},
        std::move(completionHandler));
}

void ClientImpl::startSession(
    const nx::String& desiredSessionId,
    const nx::String& targetPeerName,
    StartClientConnectSessionHandler completionHandler)
{
    using namespace std::placeholders;

    CreateClientSessionRequest request;
    request.desiredSessionId = desiredSessionId.toStdString();
    request.targetPeerName = targetPeerName.toStdString();

    issueRequest<CreateClientSessionRequest, CreateClientSessionResponse>(
        std::move(request),
        kServerClientSessionsPath,
        {targetPeerName},
        std::move(completionHandler));
}

void ClientImpl::openConnectionToTheTargetHost(
    const nx::String& sessionId,
    OpenRelayConnectionHandler completionHandler)
{
    using namespace std::placeholders;

    ConnectToPeerRequest request;
    request.sessionId = sessionId.toStdString();

    auto httpClient = prepareHttpClient<ConnectToPeerRequest, void>(
        std::move(request),
        kClientSessionConnectionsPath,
        {sessionId});
    
    post(
        [this, httpClient = std::move(httpClient), 
            completionHandler = std::move(completionHandler)]() mutable
        {
            executeOpenConnectionRequest(
                std::move(httpClient),
                std::move(completionHandler));
        });
}

SystemError::ErrorCode ClientImpl::prevRequestSysErrorCode() const
{
    return m_prevSysErrorCode;
}

void ClientImpl::stopWhileInAioThread()
{
    m_activeRequests.clear();
}

template<
    typename Request,
    typename Response,
    typename RequestPathArgument,
    typename CompletionHandler
>
void ClientImpl::issueRequest(
    Request request,
    const char* requestPathTemplate,
    std::initializer_list<RequestPathArgument> requestPathArguments,
    CompletionHandler completionHandler)
{
    auto httpClient = prepareHttpClient<Request, Response>(
        std::move(request),
        requestPathTemplate,
        std::move(requestPathArguments));

    post(
        [this, httpClient = std::move(httpClient), 
            completionHandler = std::move(completionHandler)]() mutable
        {
            executeRequest<Response>(
                std::move(httpClient), std::move(completionHandler));
        });
}

template<
    typename Request,
    typename Response,
    typename RequestPathArgument
>
std::unique_ptr<nx_http::FusionDataHttpClient<Request, Response>>
    ClientImpl::prepareHttpClient(
        Request request,
        const char* requestPathTemplate,
        std::initializer_list<RequestPathArgument> requestPathArguments)
{
    QUrl requestUrl = m_baseUrl;
    requestUrl.setPath(
        nx_http::rest::substituteParameters(
            requestPathTemplate, std::move(requestPathArguments)));

    auto httpClient = std::make_unique<
        nx_http::FusionDataHttpClient<Request, Response>>(
            requestUrl,
            m_authInfo,
            std::move(request));
    httpClient->bindToAioThread(getAioThread());
    return httpClient;
}

template<
    typename Response,
    typename HttpClient,
    typename CompletionHandler
>
void ClientImpl::executeRequest(
    HttpClient httpClient,
    CompletionHandler completionHandler)
{
    auto httpClientPtr = httpClient.get();
    m_activeRequests.push_back(std::move(httpClient));
    httpClientPtr->execute(
        [this, httpClientPtr,
            httpClientIter = std::prev(m_activeRequests.end()),
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                const nx_http::Response* httpResponse,
                Response response) mutable
        {
            m_prevSysErrorCode = sysErrorCode;
            const auto resultCode = toResultCode(sysErrorCode, httpResponse);
            m_activeRequests.erase(httpClientIter);
            completionHandler(resultCode, std::move(response));
        });
}

template<typename HttpClient, typename CompletionHandler>
void ClientImpl::executeOpenConnectionRequest(
    HttpClient httpClient,
    CompletionHandler completionHandler)
{
    auto httpClientPtr = httpClient.get();
    m_activeRequests.push_back(std::move(httpClient));
    httpClientPtr->execute(
        [this, httpClientPtr,
            httpClientIter = std::prev(m_activeRequests.end()),
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                const nx_http::Response* httpResponse) mutable
        {
            auto connection = httpClientPtr->takeSocket();
            m_prevSysErrorCode = sysErrorCode;
            const auto resultCode = toResultCode(sysErrorCode, httpResponse);
            m_activeRequests.erase(httpClientIter);
            completionHandler(resultCode, std::move(connection));
        });
}

ResultCode ClientImpl::toResultCode(
    SystemError::ErrorCode sysErrorCode,
    const nx_http::Response* httpResponse)
{
    if (sysErrorCode != SystemError::noError || !httpResponse)
        return ResultCode::networkError;

    const auto resultCodeStrIter =
        httpResponse->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
    if (resultCodeStrIter != httpResponse->headers.end())
    {
        return QnLexical::deserialized<api::ResultCode>(
            resultCodeStrIter->second,
            api::ResultCode::unknownError);
    }

    return fromHttpStatusCode(
        static_cast<nx_http::StatusCode::Value>(
            httpResponse->statusLine.statusCode));
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
