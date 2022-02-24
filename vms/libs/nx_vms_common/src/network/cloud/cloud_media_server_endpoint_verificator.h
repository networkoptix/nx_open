// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_abstract_endpoint_verificator.h>
#include <nx/network/deprecated/asynchttpclient.h>

class NX_VMS_COMMON_API CloudMediaServerEndpointVerificator:
    public nx::network::cloud::tcp::AbstractEndpointVerificator
{
    using base_type = nx::network::cloud::tcp::AbstractEndpointVerificator;

public:
    CloudMediaServerEndpointVerificator(const std::string& connectSessionId);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;

    virtual void verifyHost(
        const nx::network::SocketAddress& endpointToVerify,
        const nx::network::AddressEntry& targetHostAddress,
        nx::utils::MoveOnlyFunc<void(VerificationResult)> completionHandler) override;

    virtual SystemError::ErrorCode lastSystemErrorCode() const override;

    virtual std::unique_ptr<nx::network::AbstractStreamSocket> takeSocket() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const std::string m_connectSessionId;
    SystemError::ErrorCode m_lastSystemErrorCode = SystemError::noError;
    std::optional<std::chrono::milliseconds> m_timeout;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    nx::network::SocketAddress m_endpointToVerify;
    nx::network::AddressEntry m_targetHostAddress;
    nx::utils::MoveOnlyFunc<void(VerificationResult)> m_completionHandler;

    void onHttpRequestDone();
    bool verifyHostResponse(nx::network::http::AsyncHttpClientPtr httpClient);
};
