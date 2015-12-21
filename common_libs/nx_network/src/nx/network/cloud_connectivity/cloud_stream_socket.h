#ifndef NX_CC_CLOUD_STREAM_SOCKET_H
#define NX_CC_CLOUD_STREAM_SOCKET_H

#include <nx/utils/async_operation_guard.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace cc {

//!Socket that is able to use hole punching (tcp or udp) and mediator to establish connection
/*!
    Method to use to connect to remote peer is selected depending on route to the peer
    If connection to peer requires using udp hole punching than this socket uses UDT.
    \note Actual socket is instanciated only when address is known (\a AbstractCommunicatingSocket::connect or \a AbstractCommunicatingSocket::connectAsync)
*/
class CloudStreamSocket
:
    public AbstractStreamSocket
{
public:
    // TODO: #ak add all socket functions
    //       all configuration options should be stored in m_socketOptions

    // NOTE: all operarations shell be implemented sync or async in respect to
    //       m_socketOptions->nonBlockMode

    //!Implementation of AbstractStreamSocket::connect
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS) override;

protected:
    //!Implementation of AbstractStreamSocket::connectAsyncImpl
    virtual void connectAsyncImpl(
        const SocketAddress& address,
        std::function<void(SystemError::ErrorCode)>&& handler) override;

private:
    bool startAsyncConnect(const SocketAddress& originalAddress,
                           std::vector<AddressEntry> dnsEntries);

    std::shared_ptr<StreamSocketOptions> m_socketOptions;
    std::unique_ptr<AbstractStreamSocket> m_socketDelegate;
    std::function<void(SystemError::ErrorCode)> m_connectHandler;
    nx::utils::AsyncOperationGuard m_asyncGuard;
};

} // namespace cc
} // namespace nx

#endif  //NX_CC_CLOUD_STREAM_SOCKET_H
