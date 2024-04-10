// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/type_utils.h>

#include "../relay_api_client.h"

namespace nx::cloud::relay::api::detail {

class NX_NETWORK_API BasicClient:
    public AbstractClient
{
    using base_type = AbstractClient;

public:
    BasicClient(const nx::utils::Url& baseUrl);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void startSession(
        const std::string& desiredSessionId,
        const std::string& targetPeerName,
        StartClientConnectSessionHandler handler) override;

    virtual nx::utils::Url url() const override;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;
    virtual nx::network::http::StatusCode::Value prevRequestHttpStatusCode() const override;

    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) override;

protected:
    std::list<std::unique_ptr<network::aio::BasicPollable>> m_activeRequests;

    virtual void stopWhileInAioThread() override;

    static ResultCode toUpgradeResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx::network::http::Response* httpResponse);

    static ResultCode toResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx::network::http::Response* httpResponse);

    static ResultCode toResultCode(
        SystemError::ErrorCode sysErrorCode);

    void giveFeedback(ResultCode resultCode);

    template<
        typename Request,
        typename RequestPathArgument,
        typename CompletionHandler,
        typename ... Response
    >
        void issueUpgradeRequest(
            const std::string_view& httpMethod,
            const std::string& protocolToUpgradeTo,
            Request request,
            const char* requestPathTemplate,
            std::initializer_list<RequestPathArgument> requestPathArguments,
            CompletionHandler completionHandler);

    std::optional<std::chrono::milliseconds> timeout() const { return m_timeout; }

private:
    const nx::utils::Url m_baseUrl;
    SystemError::ErrorCode m_prevSysErrorCode = SystemError::noError;
    nx::network::http::StatusCode::Value m_prevHttpStatusCode = nx::network::http::StatusCode::undefined;
    std::optional<std::chrono::milliseconds> m_timeout;

    template<
        typename Request,
        typename Response,
        typename RequestPathArgument,
        typename CompletionHandler
    >
        void issueRequest(
            Request request,
            const char* requestPathTemplate,
            std::initializer_list<RequestPathArgument> requestPathArguments,
            CompletionHandler completionHandler);

    template<typename Request, typename Response, typename RequestPathArgument>
    std::unique_ptr<nx::network::http::FusionDataHttpClient<Request, Response>>
        prepareHttpRequest(
            Request request,
            const char* requestPathTemplate,
            std::initializer_list<RequestPathArgument> requestPathArguments);

    template<typename HttpClient, typename CompletionHandler, typename ... Response>
    void executeUpgradeRequest(
        const std::string_view& httpMethod,
        const std::string& protocolToUpgradeTo,
        HttpClient httpClient,
        CompletionHandler completionHandler);

    template<typename Response, typename HttpClient, typename CompletionHandler>
    void executeRequest(
        HttpClient httpClient,
        CompletionHandler completionHandler);

    std::string prepareActualRelayUrl(
        const std::string& contentLocationUrl,
        const std::string& requestPath);
};

//-------------------------------------------------------------------------------------------------

template<
    typename Request,
    typename RequestPathArgument,
    typename CompletionHandler,
    typename ... Response
>
void BasicClient::issueUpgradeRequest(
    const std::string_view& httpMethod,
    const std::string& protocolToUpgradeTo,
    Request request,
    const char* requestPathTemplate,
    std::initializer_list<RequestPathArgument> requestPathArguments,
    CompletionHandler completionHandler)
{
    using ResponseOrVoid = nx::utils::tuple_first_element_t<std::tuple<Response...>>;

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
void BasicClient::issueRequest(
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
BasicClient::prepareHttpRequest(
    Request request,
    const char* requestPathTemplate,
    std::initializer_list<RequestPathArgument> requestPathArguments)
{
    nx::utils::Url requestUrl = m_baseUrl;
    requestUrl.setPath(network::url::normalizePath(
        requestUrl.path().toStdString() +
        nx::network::http::rest::substituteParameters(
            requestPathTemplate, std::move(requestPathArguments))));

    auto httpClient =
        std::make_unique<nx::network::http::FusionDataHttpClient<Request, Response>>(
            requestUrl,
            nx::network::http::Credentials(),
            nx::network::ssl::kDefaultCertificateCheck,
            std::move(request));
    if (m_timeout)
        httpClient->setRequestTimeout(*m_timeout);
    httpClient->bindToAioThread(getAioThread());
    return httpClient;
}

template<typename HttpClient, typename CompletionHandler, typename ... Response>
void BasicClient::executeUpgradeRequest(
    const std::string_view& httpMethod,
    const std::string& protocolToUpgradeTo,
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
            m_prevHttpStatusCode = httpResponse
                ? httpResponse->statusLine.statusCode
                : nx::network::http::StatusCode::undefined;
            const auto resultCode = toUpgradeResultCode(sysErrorCode, httpResponse);
            m_activeRequests.erase(httpClientIter);
            completionHandler(
                resultCode,
                std::move(response)...,
                std::move(connection));
        });
}

template<typename Response, typename HttpClient, typename CompletionHandler>
void BasicClient::executeRequest(
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
            m_prevHttpStatusCode = httpResponse
                ? httpResponse->statusLine.statusCode
                : nx::network::http::StatusCode::undefined;
            const auto resultCode = toResultCode(sysErrorCode, httpResponse);
            auto requestContext = std::move(*httpClientIter);
            m_activeRequests.erase(httpClientIter);
            completionHandler(contentLocationUrl, resultCode, std::move(response));
        });
}

} // namespace nx::cloud::relay::api::detail
