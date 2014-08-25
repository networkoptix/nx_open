/**********************************************************
* 25 aug 2014
* a.kolesnikov
***********************************************************/

#include "mixed_tcp_udt_server_socket.h"

#include <QtCore/QMutexLocker>

#include "udt_socket.h"
#include "system_socket.h"



#define MSVC_EXPAND(x) x
#define GET_DO_BOOL_FUNCTION_MACRO(_1, _2, NAME, ...) NAME
#define DO_BOOL_FUNCTION(...) MSVC_EXPAND(GET_DO_BOOL_FUNCTION_MACRO(__VA_ARGS__, DO_BOOL_FUNCTION_2, DO_BOOL_FUNCTION_1)(__VA_ARGS__))

#define DO_BOOL_FUNCTION_1( Function )                                    \
    for( AbstractStreamServerSocket* serverSock : m_socketDelegates )     \
        if( !serverSock->Function() )                                     \
            return false;                                                 \
    return true;

#define DO_BOOL_FUNCTION_2( Function, Param1 )                            \
    for( AbstractStreamServerSocket* serverSock : m_socketDelegates )     \
        if( !serverSock->Function(Param1) )                               \
            return false;                                                 \
    return true;



MixedTcpUdtServerSocket::MixedTcpUdtServerSocket()
:
    m_accepting( false )
{
    m_socketDelegates.push_back(new TCPServerSocket());
    m_socketDelegates.push_back(new UdtStreamServerSocket());
}

MixedTcpUdtServerSocket::~MixedTcpUdtServerSocket()
{
    cancelAsyncIO( true );
    while( !m_queue.empty() )
    {
        delete m_queue.front().second;
        m_queue.pop();
    }
    for( AbstractStreamServerSocket* serverSock : m_socketDelegates )
        delete serverSock;
    m_socketDelegates.clear();
}

bool MixedTcpUdtServerSocket::bind(const SocketAddress& localAddress)
{
    DO_BOOL_FUNCTION(bind, localAddress)
}

SocketAddress MixedTcpUdtServerSocket::getLocalAddress() const
{
    return m_socketDelegates[0]->getLocalAddress();
}

SocketAddress MixedTcpUdtServerSocket::getPeerAddress() const
{
    return m_socketDelegates[0]->getPeerAddress();
}

void MixedTcpUdtServerSocket::close()
{
    for( AbstractStreamServerSocket* serverSock : m_socketDelegates )
        serverSock->close();
}

bool MixedTcpUdtServerSocket::isClosed() const
{
    DO_BOOL_FUNCTION(isClosed)
}

bool MixedTcpUdtServerSocket::setReuseAddrFlag(bool reuseAddr)
{
    DO_BOOL_FUNCTION(setReuseAddrFlag, reuseAddr)
}

bool MixedTcpUdtServerSocket::getReuseAddrFlag(bool* val)
{
    DO_BOOL_FUNCTION(getReuseAddrFlag, val)
}

bool MixedTcpUdtServerSocket::setNonBlockingMode(bool val)
{
    DO_BOOL_FUNCTION(setNonBlockingMode, val)
}

bool MixedTcpUdtServerSocket::getNonBlockingMode(bool* val) const
{
    DO_BOOL_FUNCTION(getNonBlockingMode, val)
}

bool MixedTcpUdtServerSocket::getMtu(unsigned int* mtuValue)
{
    //mtu is not supported for UDT
    return m_socketDelegates[0]->getMtu(mtuValue);
}

bool MixedTcpUdtServerSocket::setSendBufferSize(unsigned int buffSize)
{
    DO_BOOL_FUNCTION(setSendBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::getSendBufferSize(unsigned int* buffSize)
{
    DO_BOOL_FUNCTION(getSendBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::setRecvBufferSize(unsigned int buffSize)
{
    DO_BOOL_FUNCTION(setRecvBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::getRecvBufferSize(unsigned int* buffSize)
{
    DO_BOOL_FUNCTION(getRecvBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::setRecvTimeout(unsigned int ms)
{
    DO_BOOL_FUNCTION(setRecvTimeout, ms)
}

bool MixedTcpUdtServerSocket::getRecvTimeout(unsigned int* millis)
{
    DO_BOOL_FUNCTION(getRecvTimeout, millis)
}

bool MixedTcpUdtServerSocket::setSendTimeout(unsigned int ms)
{
    DO_BOOL_FUNCTION(setSendTimeout, ms)
}

bool MixedTcpUdtServerSocket::getSendTimeout(unsigned int* millis)
{
    DO_BOOL_FUNCTION(getSendTimeout, millis)
}

bool MixedTcpUdtServerSocket::getLastError(SystemError::ErrorCode* errorCode)
{
    //TODO #ak
    DO_BOOL_FUNCTION(getLastError, errorCode)
}

AbstractSocket::SOCKET_HANDLE MixedTcpUdtServerSocket::handle() const
{
    assert( false );
    return (AbstractSocket::SOCKET_HANDLE)(-1);
}

bool MixedTcpUdtServerSocket::listen(int queueLen)
{
    for( AbstractStreamServerSocket* serverSocket : m_socketDelegates )
        if( !serverSocket->listen(queueLen) )
            return false;
    return true;
}

AbstractStreamSocket* MixedTcpUdtServerSocket::accept()
{
    QMutexLocker lk(&m_mutex);

    if( !m_accepting )
    {
        using namespace std::placeholders;
        for( AbstractStreamServerSocket* serverSocket : m_socketDelegates )
            serverSocket->acceptAsync(std::bind(&MixedTcpUdtServerSocket::connectionAccepted, this, serverSocket, _1, _2));
        m_accepting = true;
    }

    while( m_queue.empty() )
        m_cond.wait(&m_mutex);

    std::pair<SystemError::ErrorCode, AbstractStreamSocket*> conn = m_queue.front();
    m_queue.pop();
    if( conn.first )
        SystemError::setLastErrorCode(conn.first);
    return conn.second;
}

void MixedTcpUdtServerSocket::cancelAsyncIO(bool waitForRunningHandlerCompletion)
{
    for( AbstractStreamServerSocket* serverSocket : m_socketDelegates )
        serverSocket->cancelAsyncIO(waitForRunningHandlerCompletion);
}

bool MixedTcpUdtServerSocket::acceptAsyncImpl(std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>&& /*handler*/)
{
    //TODO #ak seems like, we'll have to use AIOService directly here to avoid loosing connections in case if user does not call acceptAsync from completion handler
    assert( false );
    return false;
}

void MixedTcpUdtServerSocket::connectionAccepted(
    AbstractStreamServerSocket* serverSocket,
    SystemError::ErrorCode errorCode,
    AbstractStreamSocket* newConnection )
{
    using namespace std::placeholders;

    //TODO #ak stop accepting connections when no ones listening requires using AIOService

    QMutexLocker lk(&m_mutex);
    m_queue.push(std::make_pair(errorCode, newConnection));
    if( errorCode == SystemError::noError )
        serverSocket->acceptAsync(std::bind(&MixedTcpUdtServerSocket::connectionAccepted, this, serverSocket, _1, _2));
}
