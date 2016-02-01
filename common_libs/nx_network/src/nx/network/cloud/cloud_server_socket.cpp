#include "cloud_server_socket.h"

#include <nx/network/socket_global.h>

#include "cloud_tunnel_udt.h"

namespace nx {
namespace network {
namespace cloud {

CloudServerSocket::CloudServerSocket(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        IncomingTunnelPool* tunnelPool)
    : m_mediatorConnection(mediatorConnection)
    , m_tunnelPool(tunnelPool)
    , m_isAcceptingTunnelPool(false)
    , m_socketOptions(new StreamSocketOptions())
    , m_ioThreadSocket(SocketFactory::createStreamSocket())
{
    // TODO: #mu default values for m_socketOptions shall match default
    //           system vales: think how to implement this...
    m_socketOptions->recvTimeout = 0;
}

#ifdef CloudServerSocket_setSocketOption
    #error CloudStreamSocket_setSocketOption macro is already defined.
#endif

#define CloudServerSocket_setSocketOption(SETTER, TYPE, NAME)   \
    bool CloudServerSocket::SETTER(TYPE NAME)                   \
    {                                                           \
        m_socketOptions->NAME = NAME;                           \
        return true;                                            \
    }

#ifdef CloudServerSocket_getSocketOption
    #error CloudStreamSocket_getSocketOption macro is already defined.
#endif

#define CloudServerSocket_getSocketOption(GETTER, TYPE, NAME)   \
    bool CloudServerSocket::GETTER(TYPE* NAME) const            \
    {                                                           \
        if (!m_socketOptions->NAME)                             \
            return false;                                       \
                                                                \
        *NAME = *m_socketOptions->NAME;                         \
        return true;                                            \
    }

CloudServerSocket_setSocketOption(bind, const SocketAddress&, boundAddress)

SocketAddress CloudServerSocket::getLocalAddress() const
{
    if (!m_socketOptions->boundAddress)
        return SocketAddress();

    return *m_socketOptions->boundAddress;
}

void CloudServerSocket::close()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented...");
}

bool CloudServerSocket::isClosed() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented...");
    return false;
}

CloudServerSocket_setSocketOption(setReuseAddrFlag, bool, reuseAddrFlag)
CloudServerSocket_getSocketOption(getReuseAddrFlag, bool, reuseAddrFlag)

CloudServerSocket_setSocketOption(setSendBufferSize, unsigned int, sendBufferSize)
CloudServerSocket_getSocketOption(getSendBufferSize, unsigned int, sendBufferSize)

CloudServerSocket_setSocketOption(setRecvBufferSize, unsigned int, recvBufferSize)
CloudServerSocket_getSocketOption(getRecvBufferSize, unsigned int, recvBufferSize)

CloudServerSocket_setSocketOption(setRecvTimeout, unsigned int, recvTimeout)
CloudServerSocket_getSocketOption(getRecvTimeout, unsigned int, recvTimeout)

CloudServerSocket_setSocketOption(setSendTimeout, unsigned int, sendTimeout)
CloudServerSocket_getSocketOption(getSendTimeout, unsigned int, sendTimeout)

CloudServerSocket_setSocketOption(setNonBlockingMode, bool, nonBlockingMode)
CloudServerSocket_getSocketOption(getNonBlockingMode, bool, nonBlockingMode)

bool CloudServerSocket::getMtu(unsigned int* mtuValue) const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented...");
    return false;
}

#undef CloudServerSocket_setSocketOption
#undef CloudServerSocket_getSocketOption

bool CloudServerSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    if (m_lastError == SystemError::noError)
        return false;

    *errorCode = m_lastError;
    m_lastError = SystemError::noError;
    return true;
}

AbstractSocket::SOCKET_HANDLE CloudServerSocket::handle() const
{
    Q_ASSERT(false);
    return (AbstractSocket::SOCKET_HANDLE)(-1);
}

template<typename T>
static std::unique_ptr<T> makeAcceptor(hpm::api::ConnectionRequestedEvent& event)
{
    return std::make_unique<T>(
        String(), //< TODO: #mux get selfId from m_mediatorConnection?
        std::move(event.connectSessionID),
        std::move(event.originatingPeerID));
}

bool CloudServerSocket::listen(int queueLen)
{
    auto sharedGuard = m_asyncGuard.sharedGuard();
    m_mediatorConnection->setOnConnectionRequestedHandler([this, sharedGuard](
        hpm::api::ConnectionRequestedEvent event)
    {
        using namespace hpm::api::ConnectionMethod;
        if (event.connectionMethods & udpHolePunching)
        {
            event.connectionMethods ^= udpHolePunching;
            auto acceptor = makeAcceptor<UdtTunnelAcceptor>(event);
            acceptor->setTargetAddresses(std::move(event.udpEndpointList));
            startAcceptor(std::move(acceptor), std::move(sharedGuard));
        }

        // TODO: #mux add other connectors when supported

        if (event.connectionMethods)
            NX_LOG(lm("Unsupported ConnectionMethods: %1")
                .arg(event.connectionMethods), cl_logWARNING);
    });

    // TODO: #mux should queueLen imply any effect on TunnelPool?
    static_cast<void>(queueLen);
    return true;
}

AbstractStreamSocket* CloudServerSocket::accept()
{
    std::promise<std::pair<SystemError::ErrorCode, AbstractStreamSocket*>> promise;
    acceptAsync([&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        promise.set_value(std::make_pair(code, socket));
    });

    const auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
    {
        m_lastError = result.first;
        SystemError::setLastErrorCode(result.first);
    }

    return result.second;
}

void CloudServerSocket::pleaseStop(std::function<void()> handler)
{
    // prevent any further locks on given sharedLocks
    m_asyncGuard.reset();

    BarrierHandler barrier(std::move(handler));
    m_ioThreadSocket->pleaseStop(barrier.fork());
    for (auto& connector : m_acceptors)
        connector.second->pleaseStop(barrier.fork());
}

void CloudServerSocket::post(std::function<void()> handler)
{
    m_ioThreadSocket->post(std::move(handler));
}

void CloudServerSocket::dispatch(std::function<void()> handler)
{
    m_ioThreadSocket->dispatch(std::move(handler));
}

void CloudServerSocket::acceptAsync(
    std::function<void(SystemError::ErrorCode code,
                       AbstractStreamSocket*)> handler )
{
    Q_ASSERT_X(!m_acceptHandler, Q_FUNC_INFO, "concurent accept call");
    m_acceptHandler = std::move(handler);

    auto sharedGuard = m_asyncGuard.sharedGuard();
    m_tunnelPool->accept(m_socketOptions, [this, sharedGuard]
        (SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (auto lock = sharedGuard->lock())
        {
            Q_ASSERT_X(!m_acceptedSocket, Q_FUNC_INFO, "concurently accepted socket");
            m_lastError = code;
            m_acceptedSocket = std::move(socket);
            post([this, code]()
            {
                const auto acceptHandler = std::move(m_acceptHandler);
                m_acceptHandler = nullptr;
                acceptHandler(m_lastError, m_acceptedSocket.release());
            });
        }
    });
}

void CloudServerSocket::startAcceptor(
        std::unique_ptr<Acceptor> acceptor,
        std::shared_ptr<utils::AsyncOperationGuard::SharedGuard> sharedGuard)
{
    auto acceptorPtr = acceptor.get();
    if (auto lock = sharedGuard->lock())
    {
        const auto insert = m_acceptors.emplace(acceptorPtr, std::move(acceptor));
        Q_ASSERT(insert.second);
    }

    acceptor->accept([this, acceptorPtr, sharedGuard](
        std::unique_ptr<AbstractTunnelConnection> connection)
    {
        if (connection)
            m_tunnelPool->addNewTunnel(std::move(connection));

        if (auto lock = sharedGuard->lock())
            m_acceptors.erase(acceptorPtr);
    });
}

} // namespace cloud
} // namespace network
} // namespace nx
