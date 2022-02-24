// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/system_error.h>

#include "nx/network/aio/abstract_pollable.h"
#include "nx/network/abstract_socket.h"
#include "nx/network/aio/timer.h"
#include "nx/network/udt/udt_socket.h"

namespace nx::network::cloud::udp {

/**
 * Initiates rendezvous connection with given remote address.
 * NOTE: Instance can be safely freed within its aio thread (e.g., within completion handler).
 */
class RendezvousConnector:
    public aio::AbstractPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>
        ConnectCompletionHandler;

    /**
     * @param udpSocket If not empty, this socket is passed to udt socket.
     */
    RendezvousConnector(
        std::string connectSessionId,
        SocketAddress remotePeerAddress,
        std::unique_ptr<AbstractDatagramSocket> udpSocket);

    RendezvousConnector(
        std::string connectSessionId,
        SocketAddress remotePeerAddress,
        SocketAddress localAddressToBindTo);

    virtual ~RendezvousConnector();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler completionHandler);
    /**
     * Moves connection out of this.
     */
    virtual std::unique_ptr<nx::network::UdtStreamSocket> takeConnection();

    const std::string& connectSessionId() const;
    const SocketAddress& remoteAddress() const;

protected:
    aio::Timer m_aioThreadBinder;

private:
    const std::string m_connectSessionId;
    const SocketAddress m_remotePeerAddress;
    std::unique_ptr<AbstractDatagramSocket> m_udpSocket;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtConnection;
    ConnectCompletionHandler m_completionHandler;
    std::optional<SocketAddress> m_localAddressToBindTo;

    bool initializeUdtConnection(
        UdtStreamSocket* udtConnection,
        std::chrono::milliseconds timeout);
    void onUdtConnectFinished(SystemError::ErrorCode errorCode);
};

} // namespace nx::network::cloud::udp
