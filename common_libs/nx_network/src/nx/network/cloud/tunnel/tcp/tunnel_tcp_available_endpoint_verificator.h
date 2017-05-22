#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>

#include "tunnel_tcp_abstract_endpoint_verificator.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

class NX_NETWORK_API AvailableEndpointVerificator:
    public AbstractEndpointVerificator
{
    using base_type = AbstractEndpointVerificator;

public:
    AvailableEndpointVerificator(const nx::String& connectSessionId);

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
    const nx::String m_connectSessionId;
    boost::optional<std::chrono::milliseconds> m_timeout;
    std::unique_ptr<AbstractStreamSocket> m_connection;
    SystemError::ErrorCode m_lastSystemErrorCode = SystemError::noError;
    nx::utils::MoveOnlyFunc<void(VerificationResult)> m_completionHandler;

    void onConnectDone(SystemError::ErrorCode sysErrorCode);
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
