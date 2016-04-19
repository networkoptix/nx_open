/**********************************************************
* Apr 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <memory>

#include <utils/common/systemerror.h>

#include "nx/network/aio/abstract_pollable.h"
#include "nx/network/aio/timer.h"
#include "nx/network/udt/udt_socket.h"
#include "nx/network/system_socket.h"


namespace nx {
namespace network {
namespace cloud {
namespace udp {

/** Initiates rendezvous connection with given remote address.
    \note Instance can be safely freed within its aio thread (e.g., within completion handler)
*/
class RendezvousConnector
:
    public aio::AbstractPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<UdtStreamSocket>)> ConnectCompletionHandler;

    /**
        @param udpSocket If not empty, this socket is passed to udt socket
    */
    RendezvousConnector(
        nx::String connectSessionId,
        SocketAddress remotePeerAddress,
        std::unique_ptr<nx::network::UDPSocket> udpSocket);
    virtual ~RendezvousConnector();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler completionHandler);

    const nx::String& connectSessionId() const;
    const SocketAddress& remoteAddress() const;

protected:
    aio::Timer m_aioThreadBinder;

private:
    const nx::String m_connectSessionId;
    const SocketAddress m_remotePeerAddress;
    std::unique_ptr<nx::network::UDPSocket> m_udpSocket;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtConnection;
    ConnectCompletionHandler m_completionHandler;

    void onUdtConnectFinished(SystemError::ErrorCode errorCode);
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
