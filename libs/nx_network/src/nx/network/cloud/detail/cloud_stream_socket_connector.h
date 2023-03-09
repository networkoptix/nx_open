// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/utils/async_operation_guard.h>

#include "../any_accessible_address_connector.h"
#include "../tunnel/tunnel_attributes.h"

namespace nx::network::cloud::detail {

/**
 * Establishes connection to a given address. Goes through the Cloud if needed.
 * Effectively, it is a helper class used by CloudStreamSocket::connectAsync.
 */
class NX_NETWORK_API CloudStreamSocketConnector:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::optional<TunnelAttributes> cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection)>;

    CloudStreamSocketConnector(
        int ipVersion,
        const SocketAddress& addr);

    ~CloudStreamSocketConnector();

    virtual void bindToAioThread(aio::AbstractAioThread* t) override;

    /**
     * @param timeout Connect timeout. Zero means "infinite timeout".
     * @param attrs Applied to the established connection on success.
     */
    void connect(
        std::chrono::milliseconds timeout,
        StreamSocketAttributes attrs,
        CompletionHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    void connectToEntriesAsync(std::deque<AddressEntry> dnsEntries, int port);

private:
    const int m_ipVersion;
    const SocketAddress m_addr;
    nx::utils::AsyncOperationGuard m_guard;
    std::chrono::milliseconds m_timeout;
    StreamSocketAttributes m_attrs;
    CompletionHandler m_handler;
    std::unique_ptr<AnyAccessibleAddressConnector> m_multipleAddressConnector;
};

} // namespace nx::network::cloud::detail
