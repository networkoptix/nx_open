#ifndef NX_NETWORK_MULTIPLE_SERVER_SOCKET_H
#define NX_NETWORK_MULTIPLE_SERVER_SOCKET_H

#include "system_socket.h"
#include <utils/common/cpp14.h>
#include <queue>

namespace nx {
namespace network {

class NX_NETWORK_API MultipleServerSocket
    : public AbstractStreamServerSocket
{
protected:
    MultipleServerSocket();

public:
    ~MultipleServerSocket();

    //TODO #muskov does not compile on msvc2012
    //template<typename S1, typename S2, typename = typename std::enable_if<
    //    !(std::is_base_of<S1, S2>::value && std::is_base_of<S2, S1>::value)>::type>
    template<typename S1, typename S2>
    std::unique_ptr<AbstractStreamServerSocket> make();

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    void shutdown() override;
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
    aio::AbstractAioThread* getAioThread() override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    //!Implementation of AbstractStreamServerSocket::*
    bool listen(int queueLen) override;
    AbstractStreamSocket* accept() override;

    //!Implementation of QnStoppable::pleaseStop
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    //!Implementation of AbstractSocket::*
    void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    void acceptAsync(
        std::function<void(SystemError::ErrorCode,
                           AbstractStreamSocket*)> handler) override;

protected:
    struct NX_NETWORK_API ServerSocketHandle
    {
        std::unique_ptr<AbstractStreamServerSocket> socket;
        bool isAccepting;

        ServerSocketHandle(std::unique_ptr<AbstractStreamServerSocket> socket_);
        ServerSocketHandle(ServerSocketHandle&&);

        AbstractStreamServerSocket* operator->() const;
        void stopAccepting();
    };

    bool addSocket(std::unique_ptr<AbstractStreamServerSocket> socket);
    void accepted(ServerSocketHandle* source, SystemError::ErrorCode code,
                  AbstractStreamSocket* socket);

protected:
    bool m_nonBlockingMode;
    unsigned int m_recvTmeout;
    mutable SystemError::ErrorCode m_lastError;
    bool* m_terminated;
    std::unique_ptr<AbstractCommunicatingSocket> m_timerSocket;
    std::vector<ServerSocketHandle> m_serverSockets;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_acceptHandler;
};

template<typename S1, typename S2/*, typename*/>
std::unique_ptr<AbstractStreamServerSocket> MultipleServerSocket::make()
{
    std::unique_ptr<MultipleServerSocket> self(new MultipleServerSocket);

    if (!self->addSocket(std::unique_ptr<S1>(new S1)))
        return nullptr;

    if (!self->addSocket(std::unique_ptr<S2>(new S2)))
        return nullptr;

    return std::move(self);
}

} // namespace network
} // namespace nx

#endif // NX_NETWORK_MULTIPLE_SERVER_SOCKET_H
