// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <nx/network/abstract_socket.h>

#include "tunnel_tcp_abstract_endpoint_verificator.h"

namespace nx::network::cloud::tcp {

/**
 * Succeeds if the target endpoint accepts TCP connections.
 */
class NX_NETWORK_API AvailableEndpointVerificator:
    public AbstractEndpointVerificator
{
    using base_type = AbstractEndpointVerificator;

public:
    AvailableEndpointVerificator(const std::string& connectSessionId);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;

    virtual void verifyHost(
        const SocketAddress& endpointToVerify,
        const AddressEntry& desiredHostAddress,
        nx::utils::MoveOnlyFunc<void(VerificationResult)> completionHandler) override;

    virtual SystemError::ErrorCode lastSystemErrorCode() const override;

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const std::string m_connectSessionId;
    std::optional<std::chrono::milliseconds> m_timeout;
    std::unique_ptr<AbstractStreamSocket> m_connection;
    SystemError::ErrorCode m_lastSystemErrorCode = SystemError::noError;
    nx::utils::MoveOnlyFunc<void(VerificationResult)> m_completionHandler;

    void onConnectDone(SystemError::ErrorCode sysErrorCode);
};

} // namespace nx::network::cloud::tcp
