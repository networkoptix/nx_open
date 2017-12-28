#pragma once

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_abstract_endpoint_verificator.h>
#include <nx/network/deprecated/asynchttpclient.h>

class CloudMediaServerEndpointVerificator:
    public nx::network::cloud::tcp::AbstractEndpointVerificator
{
    using base_type = nx::network::cloud::tcp::AbstractEndpointVerificator;

public:
    CloudMediaServerEndpointVerificator(const nx::String& connectSessionId);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;

    virtual void verifyHost(
        const SocketAddress& endpointToVerify,
        const nx::network::AddressEntry& targetHostAddress,
        nx::utils::MoveOnlyFunc<void(VerificationResult)> completionHandler) override;

    virtual SystemError::ErrorCode lastSystemErrorCode() const override;

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::String m_connectSessionId;
    SystemError::ErrorCode m_lastSystemErrorCode = SystemError::noError;
    boost::optional<std::chrono::milliseconds> m_timeout;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    SocketAddress m_endpointToVerify;
    nx::network::AddressEntry m_targetHostAddress;
    nx::utils::MoveOnlyFunc<void(VerificationResult)> m_completionHandler;

    void onHttpRequestDone();
    bool verifyHostResponse(nx::network::http::AsyncHttpClientPtr httpClient);
};
