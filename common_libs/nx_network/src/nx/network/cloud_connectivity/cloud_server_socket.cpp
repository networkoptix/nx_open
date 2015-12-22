#include "cloud_server_socket.h"

#include <nx/network/socket_global.h>

namespace nx {
namespace cc {

CloudServerSocket::CloudServerSocket()
    : m_tcpSocket(new TCPServerSocket)
    , m_acceptedSocketsMaxCount(0)
{
}

bool CloudServerSocket::bind(const SocketAddress& localAddress)
{
    return m_tcpSocket->bind(localAddress);
}

SocketAddress CloudServerSocket::getLocalAddress() const
{
    return m_tcpSocket->getLocalAddress();
}

void CloudServerSocket::close()
{
    return m_tcpSocket->close();
}

bool CloudServerSocket::isClosed() const
{
    return m_tcpSocket->isClosed();
}

bool CloudServerSocket::setReuseAddrFlag(bool reuseAddr)
{
    return m_tcpSocket->setReuseAddrFlag(reuseAddr);
}

bool CloudServerSocket::getReuseAddrFlag(bool* val) const
{
    return m_tcpSocket->getReuseAddrFlag(val);
}

bool CloudServerSocket::setNonBlockingMode(bool val)
{
    return m_tcpSocket->setNonBlockingMode(val);
}

bool CloudServerSocket::getNonBlockingMode(bool* val) const
{
    return m_tcpSocket->getNonBlockingMode(val);
}

bool CloudServerSocket::getMtu(unsigned int* mtuValue) const
{
    return m_tcpSocket->getMtu(mtuValue);
}

bool CloudServerSocket::setSendBufferSize(unsigned int buffSize)
{
    return m_tcpSocket->setSendBufferSize(buffSize);
}

bool CloudServerSocket::getSendBufferSize(unsigned int* buffSize) const
{
    return m_tcpSocket->getSendBufferSize(buffSize);
}

bool CloudServerSocket::setRecvBufferSize(unsigned int buffSize)
{
    return m_tcpSocket->setRecvBufferSize(buffSize);
}

bool CloudServerSocket::getRecvBufferSize(unsigned int* buffSize) const
{
    return m_tcpSocket->getRecvBufferSize(buffSize);
}

bool CloudServerSocket::setRecvTimeout(unsigned int millis)
{
    return m_tcpSocket->setRecvTimeout(millis);
}

bool CloudServerSocket::getRecvTimeout(unsigned int* millis) const
{
    return m_tcpSocket->getRecvTimeout(millis);
}

bool CloudServerSocket::setSendTimeout(unsigned int ms)
{
    return m_tcpSocket->setSendTimeout(ms);
}

bool CloudServerSocket::getSendTimeout(unsigned int* millis) const
{
    return m_tcpSocket->getSendTimeout(millis);
}

bool CloudServerSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    // TODO: #mux probabbly we also have to take care of TunnelPool errors
    return m_tcpSocket->getLastError(errorCode);
}

AbstractSocket::SOCKET_HANDLE CloudServerSocket::handle() const
{
    // TODO: #mux fugure out what to do with TunnelPool
    return m_tcpSocket->handle();
}

bool CloudServerSocket::listen(int queueLen)
{
    {
        QnMutex m_mutex;
        m_acceptedSocketsMaxCount = queueLen;
    }

    if (!m_tcpSocket->listen(queueLen))
        return false;

    // accept new connections on the tcp socket
    m_tcpSocket->acceptAsync([this](SystemError::ErrorCode code,
                                    AbstractStreamSocket* socket)
    {
        accepted(code, std::unique_ptr<AbstractStreamSocket>(socket));
    });

    // ass well as incoming socket connections from cloud tunnels
    SocketGlobals::tunnelPool().setNewHandler(
                [this](std::shared_ptr<Tunnel> tunnel)
    {

        using namespace std::placeholders;
        tunnel->accept(std::bind(&CloudServerSocket::accepted, this, _1, _2));
    });

    return true;
}

AbstractStreamSocket* CloudServerSocket::accept()
{
    QnMutexLocker lk(&m_mutex);
    while(m_acceptedSocketsQueue.empty())
        m_acceptSyncCondition.wait(&m_mutex);

    auto socket = std::move(m_acceptedSocketsQueue.front());
    m_acceptedSocketsQueue.pop();
    return socket.release();
}

void CloudServerSocket::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));
    m_tcpSocket->pleaseStop(barrier.fork());

    // TODO: #mux use AsyncOperationGuard instead?
    SocketGlobals::tunnelPool().pleaseStop(barrier.fork());
}

void CloudServerSocket::acceptAsyncImpl(
    std::function<void(SystemError::ErrorCode code,
                       AbstractStreamSocket*)>&& handler )
{
    QnMutexLocker lk(&m_mutex);
    if (!m_acceptedSocketsQueue.empty())
    {
        auto socket = std::move(m_acceptedSocketsQueue.front());
        m_acceptedSocketsQueue.pop();

        lk.unlock();
        return handler(SystemError::noError, socket.release());
    }

    m_acceptHandlerQueue.push(std::move(handler));
}

void CloudServerSocket::accepted(SystemError::ErrorCode code,
                                 std::unique_ptr<AbstractStreamSocket> socket)
{
    QnMutexLocker lk(&m_mutex);
    if (!m_acceptHandlerQueue.empty())
    {
        auto handler = std::move(m_acceptHandlerQueue.front());
        m_acceptHandlerQueue.pop();

        lk.unlock();
        return handler(code, socket.release());
    }

    if (m_acceptedSocketsQueue.size() >= m_acceptedSocketsMaxCount)
        return socket->close();

    m_acceptedSocketsQueue.push(std::move(socket));
    m_acceptSyncCondition.wakeOne();
}

} // namespace cc
} // namespace nx
