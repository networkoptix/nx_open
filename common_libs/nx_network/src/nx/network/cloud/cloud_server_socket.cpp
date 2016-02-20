#include "cloud_server_socket.h"

#include <nx/network/socket_global.h>
#include <nx/network/stream_socket_wrapper.h>
#include <nx/network/cloud/tunnel/udp_hole_punching_acceptor.h>

namespace nx {
namespace network {
namespace cloud {

static const std::vector<CloudServerSocket::AcceptorMaker> defaultAcceptorMakers()
{
    std::vector<CloudServerSocket::AcceptorMaker> makers;

    makers.push_back([](hpm::api::ConnectionRequestedEvent& event)
    {
        using namespace hpm::api::ConnectionMethod;
        if (event.connectionMethods & udpHolePunching)
        {
            event.connectionMethods ^= udpHolePunching; //< used
            Q_ASSERT(event.udpEndpointList.size() == 1);
            if (!event.udpEndpointList.size())
                return std::unique_ptr<AbstractTunnelAcceptor>();

            auto acceptor = std::make_unique<UdpHolePunchingTunnelAcceptor>(
                std::move(event.udpEndpointList.front()));

            return std::unique_ptr<AbstractTunnelAcceptor>(std::move(acceptor));
        }

        return std::unique_ptr<AbstractTunnelAcceptor>();
    });

    // TODO: #mux add other connectors when supported
    return makers;
}

const std::vector<CloudServerSocket::AcceptorMaker>
    CloudServerSocket::kDefaultAcceptorMakers = defaultAcceptorMakers();

CloudServerSocket::CloudServerSocket(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        std::vector<AcceptorMaker> acceptorMakers)
    : m_mediatorConnection(mediatorConnection)
    , m_acceptorMakers(acceptorMakers)
    , m_terminated(false)
    , m_ioThreadSocket(new TCPSocket)
{
    // TODO: #mu default values for m_socketAttributes shall match default
    //           system vales: think how to implement this...
    m_socketAttributes.nonBlockingMode = false;
    m_socketAttributes.recvTimeout = 0;
}

CloudServerSocket::~CloudServerSocket()
{
    if (*m_socketAttributes.nonBlockingMode == false)
    {
        // Unfortunatelly we have to block here, cloud server socket uses
        // nonblocking operations even if user uses blocking mode.
        pleaseStopSync();
    }
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
    initTunnelPool(queueLen);
    m_mediatorConnection->setOnConnectionRequestedHandler(
        [this](hpm::api::ConnectionRequestedEvent event)
        {
            for (const auto& maker: m_acceptorMakers)
            {
                if (auto acceptor = maker(event))
                {
                    acceptor->setConnectionInfo(
                        event.connectSessionId, event.originatingPeerID);

                    acceptor->setMediatorConnection(m_mediatorConnection);
                    startAcceptor(std::move(acceptor));
                }
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
        promise.set_value(code);
    });

    SystemError::setLastErrorCode(promise.get_future().get());
    return acceptedSocket.release();
}

void CloudServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    auto stop = [this, handler = std::move(handler)]() mutable
    {
        if (m_mediatorConnection)
            m_mediatorConnection.reset();

        BarrierHandler barrier(
            [this, handler = std::move(handler)]() mutable
            {
                // 3rd - Stop Tunnel Pool and IO thread
                BarrierHandler barrier(std::move(handler));
                m_ioThreadSocket->pleaseStop(barrier.fork());
                if (m_tunnelPool)
                    m_tunnelPool->pleaseStop(barrier.fork());

            });

        // 2nd - Stop Acceptors
        for (auto& acceptor : m_acceptors)
            acceptor->pleaseStop(barrier.fork());
    };

    // 1st - Stop Indications
    if (m_mediatorConnection)
        m_mediatorConnection->pleaseStop(std::move(stop));
    else
        stop();
}

void CloudServerSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_ioThreadSocket->post(std::move(handler));
}

void CloudServerSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
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
                       AbstractStreamSocket*)> handler)
{
    m_tunnelPool->getNextSocketAsync(
        [this, handler](std::unique_ptr<AbstractStreamSocket> socket)
        {
            Q_ASSERT_X(!m_acceptedSocket, Q_FUNC_INFO, "concurrently accepted socket");
            NX_LOGX(lm("accepted socket %1").arg(socket), cl_logDEBUG2);

            m_acceptedSocket = std::move(socket);
            m_ioThreadSocket->post(
                [this, handler]()
                {
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
                });
        },
        m_socketAttributes.recvTimeout);
}

void CloudServerSocket::initTunnelPool(int queueLen)
{
    m_tunnelPool.reset(new IncomingTunnelPool(
        m_ioThreadSocket->getAioThread(), queueLen));
}

void CloudServerSocket::startAcceptor(
        std::unique_ptr<AbstractTunnelAcceptor> acceptor)
{
    auto acceptorPtr = acceptor.get();
    {
        QnMutexLocker lock(&m_mutex);
        m_acceptors.push_back(std::move(acceptor));
    }

    acceptorPtr->accept(
        [this, acceptorPtr](
            SystemError::ErrorCode code,
            std::unique_ptr<AbstractIncomingTunnelConnection> connection)
        {
            NX_LOGX(lm("acceptor %1 returned %2: %3")
                    .arg(acceptorPtr).arg(connection)
                    .arg(SystemError::toString(code)), cl_logDEBUG2);

            {
                QnMutexLocker lock(&m_mutex);
                if (m_terminated)
                    return; // will wait for pleaseStop handler

                const auto it = std::find_if(
                    m_acceptors.begin(), m_acceptors.end(),
                    [&](const std::unique_ptr<AbstractTunnelAcceptor>& a)
                    { return a.get() == acceptorPtr; });

                Q_ASSERT_X(it != m_acceptors.end(), Q_FUNC_INFO,
                           "Is acceptor already dead?");

                m_acceptors.erase(it);
            }

            if (code == SystemError::noError)
                m_tunnelPool->addNewTunnel(std::move(connection));
        });
}

} // namespace cloud
} // namespace network
} // namespace nx
