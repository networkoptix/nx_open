#ifndef NX_NETWORK_MULTIPLE_SERVER_SOCKET_H
#define NX_NETWORK_MULTIPLE_SERVER_SOCKET_H

#include <nx/network/system_socket.h>
#include <utils/common/cpp14.h>
#include <queue>

namespace nx {
namespace network {

class MultipleServerSocket
    : public AbstractStreamServerSocket
{
    MultipleServerSocket();

public:
    // TODO: #mux reimplement with variadic template
    template<typename S1, typename S2>
    static std::unique_ptr<AbstractStreamServerSocket> make();

    ~MultipleServerSocket();

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

protected:
    //!Implementation of AbstractSocket::*
    void postImpl( std::function<void()>&& handler ) override;
    void dispatchImpl( std::function<void()>&& handler ) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsyncImpl
    void acceptAsyncImpl(
        std::function<void(SystemError::ErrorCode,
                           AbstractStreamSocket*)>&& handler) override;

private:
    struct ServerSocketData
    {
        std::unique_ptr<AbstractStreamServerSocket> socket;
        bool isAccepting;

        ServerSocketData(std::unique_ptr<AbstractStreamServerSocket> socket_);
        ServerSocketData(ServerSocketData&&);

        AbstractStreamServerSocket* operator->() const;
    };

    struct AcceptedData
    {
          SystemError::ErrorCode code;
          std::unique_ptr<AbstractStreamSocket> socket;
          std::chrono::time_point<std::chrono::system_clock> time;

          AcceptedData(SystemError::ErrorCode code_,
                       AbstractStreamSocket* socket_);
          AcceptedData(AcceptedData&&);
    };

    bool addSocket(std::unique_ptr<AbstractStreamServerSocket> socket);
    void accepted(ServerSocketData* source, SystemError::ErrorCode code,
                  AbstractStreamSocket* socket);

private:
    bool m_nonBlockingMode;
    std::vector<ServerSocketData> m_serverSockets;

    mutable QnMutex m_mutex;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_acceptHandler;
    std::queue<AcceptedData> m_acceptedInfos;

};

// TODO: #mux reimplement with variadic template
template<typename S1, typename S2>
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
