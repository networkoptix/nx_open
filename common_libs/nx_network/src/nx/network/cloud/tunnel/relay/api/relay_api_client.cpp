#include "relay_api_client.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/type_utils.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/url/url_parse_helper.h>

#include "relay_api_http_paths.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {

//-------------------------------------------------------------------------------------------------
// ClientFactory

static ClientFactory::CustomFactoryFunc factoryFunc;

std::unique_ptr<Client> ClientFactory::create(const utils::Url &baseUrl)
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

ClientImpl::ClientImpl(const nx::utils::Url& baseUrl):
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

    issueUpgradeRequest<
        BeginListeningRequest, nx::String, BeginListeningHandler, BeginListeningResponse>(
            nx::network::http::Method::post,
            kRelayProtocolName,
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

    const auto requestPath = nx::network::http::rest::substituteParameters(
        kServerClientSessionsPath, {targetPeerName});

    issueRequest<CreateClientSessionRequest, CreateClientSessionResponse>(
        std::move(request),
        kServerClientSessionsPath,
        {targetPeerName},
        [completionHandler = std::move(completionHandler), requestPath](
            const std::string contentLocationUrl,
            ResultCode resultCode,
            CreateClientSessionResponse response)
        {
            response.actualRelayUrl = contentLocationUrl;
            // Removing request path from the end of response.actualRelayUrl
            // so that we have basic relay url.
            NX_ASSERT(
                response.actualRelayUrl.find(requestPath) != std::string::npos);
            NX_ASSERT(
                response.actualRelayUrl.find(requestPath) + requestPath.size() ==
                response.actualRelayUrl.size());

            response.actualRelayUrl.erase(
                response.actualRelayUrl.size() - requestPath.size());
            completionHandler(resultCode, std::move(response));
        });
}

void ClientImpl::openConnectionToTheTargetHost(
    const nx::String& sessionId,
    OpenRelayConnectionHandler completionHandler)
{
    using namespace std::placeholders;

    ConnectToPeerRequest request;
    request.sessionId = sessionId.toStdString();

    issueUpgradeRequest<
        ConnectToPeerRequest, nx::String, OpenRelayConnectionHandler>(
            nx::network::http::Method::post,
            kRelayProtocolName,
            std::move(request),
            kClientSessionConnectionsPath,
            {sessionId},
            std::move(completionHandler));
}

nx::utils::Url ClientImpl::url() const
{
    return m_baseUrl;
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
    typename RequestPathArgument,
    typename CompletionHandler,
    typename ... Response
>
void ClientImpl::issueUpgradeRequest(
    nx::network::http::Method::ValueType httpMethod,
    const nx::network::http::StringType& protocolToUpgradeTo,
    Request request,
    const char* requestPathTemplate,
    std::initializer_list<RequestPathArgument> requestPathArguments,
    CompletionHandler completionHandler)
{
    using ResponseOrVoid =
        typename nx::utils::tuple_first_element<void, std::tuple<Response...>>::type;

    auto httpClient = prepareHttpRequest<Request, ResponseOrVoid>(
        std::move(request),
        requestPathTemplate,
        std::move(requestPathArguments));

    post(
        [this, httpMethod, protocolToUpgradeTo, httpClient = std::move(httpClient),
            completionHandler = std::move(completionHandler)]() mutable
        {
            executeUpgradeRequest<
                decltype(httpClient), decltype(completionHandler), Response...>(
                    httpMethod,
                    protocolToUpgradeTo,
                    std::move(httpClient),
                    std::move(completionHandler));
        });
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
    auto httpClient = prepareHttpRequest<Request, Response>(
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

template<typename Request, typename Response, typename RequestPathArgument>
std::unique_ptr<nx::network::http::FusionDataHttpClient<Request, Response>>
    ClientImpl::prepareHttpRequest(
        Request request,
        const char* requestPathTemplate,
        std::initializer_list<RequestPathArgument> requestPathArguments)
{
    nx::utils::Url requestUrl = m_baseUrl;
    requestUrl.setPath(network::url::normalizePath(
        requestUrl.path() +
        nx::network::http::rest::substituteParameters(
            requestPathTemplate, std::move(requestPathArguments)).c_str()));

    auto httpClient = std::make_unique<
        nx::network::http::FusionDataHttpClient<Request, Response>>(
            requestUrl,
            m_authInfo,
            std::move(request));
    httpClient->bindToAioThread(getAioThread());
    return httpClient;
}

template<typename HttpClient, typename CompletionHandler, typename ... Response>
void ClientImpl::executeUpgradeRequest(
    nx::network::http::Method::ValueType httpMethod,
    const nx::network::http::StringType& protocolToUpgradeTo,
    HttpClient httpClient,
    CompletionHandler completionHandler)
{
    auto httpClientPtr = httpClient.get();
    m_activeRequests.push_back(std::move(httpClient));
    httpClientPtr->executeUpgrade(
        httpMethod,
        protocolToUpgradeTo,
        [this, httpClientPtr,
            httpClientIter = std::prev(m_activeRequests.end()),
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                const nx::network::http::Response* httpResponse,
                Response ... response) mutable
        {
            auto connection = httpClientPtr->takeSocket();
            auto httpClient = std::move(*httpClientIter);
            m_prevSysErrorCode = sysErrorCode;
            const auto resultCode = toUpgradeResultCode(sysErrorCode, httpResponse);
            m_activeRequests.erase(httpClientIter);
            completionHandler(
                resultCode,
                std::move(response)...,
                std::move(connection));
        });
}

template<typename Response, typename HttpClient, typename CompletionHandler>
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
                const nx::network::http::Response* httpResponse,
                Response response) mutable
        {
            auto contentLocationUrl =
                httpClientPtr->httpClient().contentLocationUrl().toString().toStdString();
            m_prevSysErrorCode = sysErrorCode;
            const auto resultCode = toResultCode(sysErrorCode, httpResponse);
            m_activeRequests.erase(httpClientIter);
            completionHandler(contentLocationUrl, resultCode, std::move(response));
        });
}

ResultCode ClientImpl::toUpgradeResultCode(
    SystemError::ErrorCode sysErrorCode,
    const nx::network::http::Response* httpResponse)
{
    if (sysErrorCode != SystemError::noError || !httpResponse)
        return ResultCode::networkError;

    if (httpResponse->statusLine.statusCode == nx::network::http::StatusCode::switchingProtocols)
        return ResultCode::ok;

    const auto resultCode = toResultCode(sysErrorCode, httpResponse);
    if (resultCode == api::ResultCode::ok)
    {
        // Server did not upgrade connection, but reported OK. It is unexpected...
        return api::ResultCode::unknownError;
    }
    return resultCode;
}

ResultCode ClientImpl::toResultCode(
    SystemError::ErrorCode sysErrorCode,
    const nx::network::http::Response* httpResponse)
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
        static_cast<nx::network::http::StatusCode::Value>(
            httpResponse->statusLine.statusCode));
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
