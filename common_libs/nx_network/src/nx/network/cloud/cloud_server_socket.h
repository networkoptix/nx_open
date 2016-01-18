#ifndef nx_cc_cloud_server_socket_h
#define nx_cc_cloud_server_socket_h

#include <nx/utils/async_operation_guard.h>
#include <nx/network/abstract_socket.h>

#include "cloud_tunnel.h"

namespace nx {
namespace network {
namespace cloud {

//!Accepts connections incoming via mediator
/*!
    Listening hostname is reported to the mediator to listen on.
    \todo #ak what listening port should mean in this case?
*/
class NX_NETWORK_API CloudServerSocket
:
    public AbstractStreamServerSocket
{
public:
    CloudServerSocket();

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    bool isClosed() const override;
    bool setReuseAddrFlag(bool reuseAddr) override;
    bool getReuseAddrFlag(bool* val) const override;
    bool setNonBlockingMode(bool val) override;
    bool getNonBlockingMode(bool* val) const override;
    bool getMtu(unsigned int* mtuValue) const override;
    bool setSendBufferSize(unsigned int buffSize) override;
    bool getSendBufferSize(unsigned int* buffSize) const override;
    bool setRecvBufferSize(unsigned int buffSize) override;
    bool getRecvBufferSize(unsigned int* buffSize) const override;
    bool setRecvTimeout(unsigned int millis) override;
    bool getRecvTimeout(unsigned int* millis) const override;
    bool setSendTimeout(unsigned int ms) override;
    bool getSendTimeout(unsigned int* millis) const override;
    bool getLastError(SystemError::ErrorCode* errorCode) const override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of AbstractStreamServerSocket::*
    bool listen(int queueLen) override;
    AbstractStreamSocket* accept() override;

    //!Implementation of QnStoppable::pleaseStop
    void pleaseStop(std::function<void()> handler) override;

    //!Implementation of AbstractSocket::*
    void post(std::function<void()> handler) override;
    void dispatch(std::function<void()> handler) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    void acceptAsync(
        std::function<void(SystemError::ErrorCode,
                           AbstractStreamSocket*)> handler) override;

private:
    void postAcceptImpl();

    bool m_isAcceptingTunnelPool;
    mutable SystemError::ErrorCode m_lastError;
    std::shared_ptr<StreamSocketOptions> m_socketOptions;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_acceptHandler;
    std::unique_ptr<AbstractStreamSocket> m_acceptedSocket;
    nx::utils::AsyncOperationGuard m_asyncGuard;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif  //nx_cc_cloud_server_socket_h
