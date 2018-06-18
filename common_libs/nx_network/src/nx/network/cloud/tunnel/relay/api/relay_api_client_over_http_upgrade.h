#pragma once

#include <nx/network/http/auth_cache.h>
#include <nx/network/http/fusion_data_http_client.h>

#include "relay_api_client.h"

namespace nx::cloud::relay::api {

class NX_NETWORK_API ClientOverHttpUpgrade:
    public Client
{
    using base_type = Client;

public:
    ClientOverHttpUpgrade(
        const nx::utils::Url& baseUrl,
        ClientFeedbackFunction /*feedbackFunction*/);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void beginListening(
        const nx::String& peerName,
        BeginListeningHandler completionHandler) override;

    virtual void startSession(
        const nx::String& desiredSessionId,
        const nx::String& targetPeerName,
        StartClientConnectSessionHandler handler) override;

    virtual void openConnectionToTheTargetHost(
        const nx::String& sessionId,
        OpenRelayConnectionHandler handler) override;

    virtual nx::utils::Url url() const override;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

protected:
    std::list<std::unique_ptr<network::aio::BasicPollable>> m_activeRequests;

    virtual void stopWhileInAioThread() override;

    ResultCode toResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx::network::http::Response* httpResponse);

private:
    const nx::utils::Url m_baseUrl;
    SystemError::ErrorCode m_prevSysErrorCode;
    nx::network::http::AuthInfo m_authInfo;

    template<
        typename Request,
        typename RequestPathArgument,
        typename CompletionHandler,
        typename ... Response
    >
    void issueUpgradeRequest(
        nx::network::http::Method::ValueType httpMethod,
        const nx::network::http::StringType& protocolToUpgradeTo,
        Request request,
        const char* requestPathTemplate,
        std::initializer_list<RequestPathArgument> requestPathArguments,
        CompletionHandler completionHandler);

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
        nx::network::http::Method::ValueType httpMethod,
        const nx::network::http::StringType& protocolToUpgradeTo,
        HttpClient httpClient,
        CompletionHandler completionHandler);

    template<typename Response, typename HttpClient, typename CompletionHandler>
    void executeRequest(
        HttpClient httpClient,
        CompletionHandler completionHandler);

    ResultCode toUpgradeResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx::network::http::Response* httpResponse);
};

} // namespace nx::cloud::relay::api
