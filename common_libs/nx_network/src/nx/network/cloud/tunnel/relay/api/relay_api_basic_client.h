#pragma once

#include <nx/network/http/auth_cache.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/type_utils.h>

#include "relay_api_client.h"

namespace nx::cloud::relay::api {

class NX_NETWORK_API BasicClient:
    public Client
{
    using base_type = Client;

public:
    BasicClient(
        const QUrl& baseUrl,
        ClientFeedbackFunction /*feedbackFunction*/);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void startSession(
        const std::string& desiredSessionId,
        const std::string& targetPeerName,
        StartClientConnectSessionHandler handler) override;

    virtual QUrl url() const override;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

protected:
    std::list<std::unique_ptr<network::aio::BasicPollable>> m_activeRequests;

    virtual void stopWhileInAioThread() override;

    ResultCode toResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx_http::Response* httpResponse);

    void giveFeedback(ResultCode resultCode);

    template<
        typename Request,
        typename RequestPathArgument,
        typename CompletionHandler,
        typename ... Response
    >
        void issueUpgradeRequest(
            nx_http::Method::ValueType httpMethod,
            const std::string& protocolToUpgradeTo,
            Request request,
            const char* requestPathTemplate,
            std::initializer_list<RequestPathArgument> requestPathArguments,
            CompletionHandler completionHandler);

private:
    const QUrl m_baseUrl;
    ClientFeedbackFunction m_feedbackFunction;
    SystemError::ErrorCode m_prevSysErrorCode;
    nx_http::AuthInfo m_authInfo;

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
    std::unique_ptr<nx_http::FusionDataHttpClient<Request, Response>>
        prepareHttpRequest(
            Request request,
            const char* requestPathTemplate,
            std::initializer_list<RequestPathArgument> requestPathArguments);

    template<typename HttpClient, typename CompletionHandler, typename ... Response>
    void executeUpgradeRequest(
        nx_http::Method::ValueType httpMethod,
        const std::string& protocolToUpgradeTo,
        HttpClient httpClient,
        CompletionHandler completionHandler);

    template<typename Response, typename HttpClient, typename CompletionHandler>
    void executeRequest(
        HttpClient httpClient,
        CompletionHandler completionHandler);

    ResultCode toUpgradeResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx_http::Response* httpResponse);

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
    nx_http::Method::ValueType httpMethod,
    const std::string& protocolToUpgradeTo,
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
std::unique_ptr<nx_http::FusionDataHttpClient<Request, Response>>
BasicClient::prepareHttpRequest(
    Request request,
    const char* requestPathTemplate,
    std::initializer_list<RequestPathArgument> requestPathArguments)
{
    QUrl requestUrl = m_baseUrl;
    requestUrl.setPath(network::url::normalizePath(
        requestUrl.path() +
        nx_http::rest::substituteParameters(
            requestPathTemplate, std::move(requestPathArguments))));

    auto httpClient = std::make_unique<
        nx_http::FusionDataHttpClient<Request, Response>>(
            requestUrl,
            m_authInfo,
            std::move(request));
    httpClient->bindToAioThread(getAioThread());
    return httpClient;
}

template<typename HttpClient, typename CompletionHandler, typename ... Response>
void BasicClient::executeUpgradeRequest(
    nx_http::Method::ValueType httpMethod,
    const std::string& protocolToUpgradeTo,
    HttpClient httpClient,
    CompletionHandler completionHandler)
{
    auto httpClientPtr = httpClient.get();
    m_activeRequests.push_back(std::move(httpClient));
    httpClientPtr->executeUpgrade(
        httpMethod,
        protocolToUpgradeTo.c_str(),
        [this, httpClientPtr,
        httpClientIter = std::prev(m_activeRequests.end()),
        completionHandler = std::move(completionHandler)](
            SystemError::ErrorCode sysErrorCode,
            const nx_http::Response* httpResponse,
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
            const nx_http::Response* httpResponse,
            Response response) mutable
        {
            auto contentLocationUrl =
                httpClientPtr->httpClient().contentLocationUrl().toString().toStdString();
            m_prevSysErrorCode = sysErrorCode;
            const auto resultCode = toResultCode(sysErrorCode, httpResponse);
            auto requestContext = std::move(*httpClientIter);
            m_activeRequests.erase(httpClientIter);
            completionHandler(contentLocationUrl, resultCode, std::move(response));
        });
}

} // namespace nx::cloud::relay::api
