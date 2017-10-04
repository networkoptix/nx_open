#pragma once

#include <list>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/utils/move_only_func.h>

#include "relay_api_data_types.h"
#include "relay_api_result_code.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {

//-------------------------------------------------------------------------------------------------
// Client

using BeginListeningHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode,
        BeginListeningResponse,
        std::unique_ptr<AbstractStreamSocket>)>;

using StartClientConnectSessionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode, CreateClientSessionResponse)>;

using OpenRelayConnectionHandler = 
    nx::utils::MoveOnlyFunc<void(ResultCode, std::unique_ptr<AbstractStreamSocket>)>;

class NX_NETWORK_API Client:
    public network::aio::BasicPollable
{
public:
    virtual void beginListening(
        const nx::String& peerName,
        BeginListeningHandler completionHandler) = 0;
    /**
     * @param desiredSessionId Can be empty. 
     *   In this case server will generate unique session id itself.
     */
    virtual void startSession(
        const nx::String& desiredSessionId,
        const nx::String& targetPeerName,
        StartClientConnectSessionHandler handler) = 0;
    /**
     * After successful call socket provided represents connection to the requested peer.
     */
    virtual void openConnectionToTheTargetHost(
        const nx::String& sessionId,
        OpenRelayConnectionHandler handler) = 0;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const = 0;
};

//-------------------------------------------------------------------------------------------------
// ClientFactory

class NX_NETWORK_API ClientFactory
{
public:
    using CustomFactoryFunc = 
        nx::utils::MoveOnlyFunc<std::unique_ptr<Client>(const nx::utils::Url&)>;

    static std::unique_ptr<Client> create(const nx::utils::Url& baseUrl);

    static CustomFactoryFunc setCustomFactoryFunc(CustomFactoryFunc newFactoryFunc);
};

//-------------------------------------------------------------------------------------------------
// ClientImpl

class NX_NETWORK_API ClientImpl:
    public Client
{
    using base_type = Client;

public:
    ClientImpl(const nx::utils::Url& baseUrl);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread);

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

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

private:
    const nx::utils::Url m_baseUrl;
    SystemError::ErrorCode m_prevSysErrorCode;
    nx_http::AuthInfo m_authInfo;
    std::list<std::unique_ptr<network::aio::BasicPollable>> m_activeRequests;

    virtual void stopWhileInAioThread() override;

    template<
        typename Request,
        typename RequestPathArgument,
        typename CompletionHandler,
        typename ... Response
    >
    void issueUpgradeRequest(
        nx_http::Method::ValueType httpMethod,
        const nx_http::StringType& protocolToUpgradeTo,
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
    std::unique_ptr<nx_http::FusionDataHttpClient<Request, Response>>
        prepareHttpRequest(
            Request request,
            const char* requestPathTemplate,
            std::initializer_list<RequestPathArgument> requestPathArguments);

    template<typename HttpClient, typename CompletionHandler, typename ... Response>
    void executeUpgradeRequest(
        nx_http::Method::ValueType httpMethod,
        const nx_http::StringType& protocolToUpgradeTo,
        HttpClient httpClient,
        CompletionHandler completionHandler);

    template<typename Response, typename HttpClient, typename CompletionHandler>
    void executeRequest(
        HttpClient httpClient,
        CompletionHandler completionHandler);

    ResultCode toUpgradeResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx_http::Response* httpResponse);

    ResultCode toResultCode(
        SystemError::ErrorCode sysErrorCode,
        const nx_http::Response* httpResponse);
};

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
