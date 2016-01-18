#include "cloud_server_socket.h"

#include <nx/network/socket_global.h>

namespace nx {
namespace network {
namespace cloud {

CloudServerSocket::CloudServerSocket()
    : m_isAcceptingTunnelPool(false)
    , m_socketOptions(new StreamSocketOptions())
    , m_ioThreadSocket(SocketFactory::createStreamSocket())
{
    // TODO: #mu default values for m_socketOptions shell match default
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

bool CloudServerSocket::listen(int queueLen)
{
    // TODO: #mux Shell queueLen imply any effect on TunnelPool?
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
    m_asyncGuard.reset();
    m_ioThreadSocket->pleaseStop(std::move(handler));
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
    SocketGlobals::tunnelPool().accept(m_socketOptions, [this, sharedGuard]
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

} // namespace cloud
} // namespace network
} // namespace nx
