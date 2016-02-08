#include "cloud_server_socket.h"

#include <nx/network/socket_global.h>
#include <nx/network/stream_socket_wrapper.h>

#include "cloud_tunnel_udt.h"

namespace nx {
namespace network {
namespace cloud {

static const std::vector<CloudServerSocket::AcceptorMaker> defaultAcceptorMakers()
{
    std::vector<CloudServerSocket::AcceptorMaker> makers;

    makers.push_back([](
        const String& selfPeerId, hpm::api::ConnectionRequestedEvent& event)
    {
        using namespace hpm::api::ConnectionMethod;
        if (event.connectionMethods & udpHolePunching == 0)
            return std::unique_ptr<AbstractTunnelAcceptor>();

        event.connectionMethods ^= udpHolePunching; //< used
        auto acceptor = std::make_unique<UdtTunnelAcceptor>(
                std::move(selfPeerId), event.connectSessionID, event.originatingPeerID);

        acceptor->setTargetAddresses(std::move(event.udpEndpointList));
        return std::unique_ptr<AbstractTunnelAcceptor>(std::move(acceptor));
    });

    // TODO: #mux add other connectors when supported
    return makers;
}

const std::vector<CloudServerSocket::AcceptorMaker>
    CloudServerSocket::kDefaultAcceptorMakers = defaultAcceptorMakers();

CloudServerSocket::CloudServerSocket(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        IncomingTunnelPool* tunnelPool,
        std::vector<AcceptorMaker> acceptorMakers)
    : m_mediatorConnection(mediatorConnection)
    , m_acceptorMakers(acceptorMakers)
    , m_tunnelPool(tunnelPool ? tunnelPool : &SocketGlobals::incomingTunnelPool())
    , m_ioThreadSocket(new TCPSocket())
    , m_timerThreadSocket(new TCPSocket())
{
    m_timerThreadSocket->bindToAioThread(m_ioThreadSocket->getAioThread());

    // TODO: #mu default values for m_socketAttributes shall match default
    //           system vales: think how to implement this...
    m_socketAttributes.recvTimeout = 0;
}

bool CloudServerSocket::bind(const SocketAddress& localAddress)
{
    // Does not make any sense in cloud socket context
    static_cast<void>(localAddress);
    return true;
}

SocketAddress CloudServerSocket::getLocalAddress() const
{
    // TODO: #mux Figure out if it causes any problems
    return SocketAddress();
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

void CloudServerSocket::shutdown()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented...");
}

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

bool CloudServerSocket::listen(int queueLen)
{
    m_tunnelPool->setAcceptLimit(queueLen);

    auto sharedGuard = m_asyncGuard.sharedGuard();
    m_mediatorConnection->setOnConnectionRequestedHandler([this, sharedGuard](
        hpm::api::ConnectionRequestedEvent event)
    {
        if (const auto lock = sharedGuard->lock())
        {
            const auto selfPeerId = m_mediatorConnection->selfPeerId();
            for (const auto& maker: m_acceptorMakers)
                if (auto acceptor = maker(selfPeerId, event))
                    startAcceptor(std::move(acceptor));
        }

        if (event.connectionMethods)
            NX_LOG(lm("Unsupported ConnectionMethods: %1")
                .arg(event.connectionMethods), cl_logWARNING);
    });

    return true;
}

AbstractStreamSocket* CloudServerSocket::accept()
{
    if (m_socketAttributes.nonBlockingMode && *m_socketAttributes.nonBlockingMode)
    {
        if (auto socket = m_tunnelPool->getNextSocketIfAny())
            return new StreamSocketWrapper(std::move(socket));

        SystemError::setLastErrorCode(SystemError::wouldBlock);
        return nullptr;
    }

    std::promise<SystemError::ErrorCode> promise;
    std::unique_ptr<AbstractStreamSocket> acceptedSocket;
    acceptAsync([&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        acceptedSocket.reset(socket);
        post([&promise, code](){ promise.set_value(code); } );
    });

    SystemError::setLastErrorCode(promise.get_future().get());
    return acceptedSocket.release();
}

void CloudServerSocket::pleaseStop(std::function<void()> handler)
{
    // prevent any further locks on given sharedLocks
    m_asyncGuard.reset();

    BarrierHandler barrier([this, handler]()
    {
        m_acceptors.clear();
        handler();
    });

    m_ioThreadSocket->pleaseStop(barrier.fork());
    m_timerThreadSocket->pleaseStop(barrier.fork());

    for (auto& connector : m_acceptors)
        connector->pleaseStop(barrier.fork());
}

void CloudServerSocket::post(std::function<void()> handler)
{
    m_ioThreadSocket->post(std::move(handler));
}

void CloudServerSocket::dispatch(std::function<void()> handler)
{
    m_ioThreadSocket->dispatch(std::move(handler));
}

aio::AbstractAioThread* CloudServerSocket::getAioThread()
{
    return m_ioThreadSocket->getAioThread();
}

void CloudServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_ioThreadSocket->bindToAioThread(aioThread);
}

void CloudServerSocket::acceptAsync(
    std::function<void(SystemError::ErrorCode code,
                       AbstractStreamSocket*)> handler )
{
    Q_ASSERT_X(!m_acceptHandler, Q_FUNC_INFO, "concurrent accept call");
    m_acceptHandler = std::move(handler);
    NX_LOGX(lm("accept async"), cl_logDEBUG2);

    auto sharedGuard = m_asyncGuard.sharedGuard();
    m_tunnelPool->getNextSocketAsync([this, sharedGuard]
        (std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (auto lock = sharedGuard->lock())
        {
            Q_ASSERT_X(!m_acceptedSocket, Q_FUNC_INFO, "concurrently accepted socket");
            NX_LOGX(lm("accepted socket %1").arg(socket), cl_logDEBUG2);

            m_acceptedSocket = std::move(socket);
            m_timerThreadSocket->post([this](){ callAcceptHandler(); });
        }
    });

    unsigned int timeout;
    if (getRecvTimeout(&timeout) && timeout != 0)
        m_timerThreadSocket->registerTimer(
            timeout, [this](){ callAcceptHandler(); });
}

void CloudServerSocket::startAcceptor(
        std::unique_ptr<AbstractTunnelAcceptor> acceptor)
{
    auto acceptorPtr = acceptor.get();
    m_acceptors.push_back(std::move(acceptor));

    auto sharedGuard = m_asyncGuard.sharedGuard();
    acceptorPtr->accept([this, acceptorPtr, sharedGuard](
        std::unique_ptr<AbstractTunnelConnection> connection)
    {
        if (connection)
            m_tunnelPool->addNewTunnel(std::move(connection));

        if (auto lock = sharedGuard->lock())
        {
            const auto it = std::find_if(
                m_acceptors.begin(), m_acceptors.end(),
                [&](const std::unique_ptr<AbstractTunnelAcceptor>& a)
                { return a.get() == acceptorPtr; });

            Q_ASSERT_X(it != m_acceptors.end(), Q_FUNC_INFO,
                       "Is acceptor already dead?");

            m_acceptors.erase(it);
        }
    });
}

void CloudServerSocket::callAcceptHandler()
{
    m_timerThreadSocket->pleaseStopSync();

    auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;

    auto socket = std::move(m_acceptedSocket);
    m_acceptedSocket = nullptr;

    if (socket)
    {
        NX_LOGX(lm("return socket %1").arg(socket), cl_logDEBUG2);
        handler(SystemError::noError, socket.release());
    }
    else
    {
        NX_LOGX(lm("accept timed out"), cl_logDEBUG2);
        handler(SystemError::timedOut, nullptr);
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
