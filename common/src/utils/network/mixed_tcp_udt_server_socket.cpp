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


//TODO #ak looks like this class needs to be re-written using aio::AIOService

MixedTcpUdtServerSocket::MixedTcpUdtServerSocket()
:
    m_inSyncAccept( false )
{
    m_socketDelegates.push_back(new TCPServerSocket());
    m_socketDelegates.push_back(new UdtStreamServerSocket());

    m_acceptingFlags.resize( m_socketDelegates.size() );
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

bool MixedTcpUdtServerSocket::getReuseAddrFlag(bool* val) const
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

bool MixedTcpUdtServerSocket::getMtu(unsigned int* mtuValue) const
{
    //mtu is not supported for UDT
    return m_socketDelegates[0]->getMtu(mtuValue);
}

bool MixedTcpUdtServerSocket::setSendBufferSize(unsigned int buffSize)
{
    DO_BOOL_FUNCTION(setSendBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::getSendBufferSize(unsigned int* buffSize) const
{
    DO_BOOL_FUNCTION(getSendBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::setRecvBufferSize(unsigned int buffSize)
{
    DO_BOOL_FUNCTION(setRecvBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::getRecvBufferSize(unsigned int* buffSize) const
{
    DO_BOOL_FUNCTION(getRecvBufferSize, buffSize)
}

bool MixedTcpUdtServerSocket::setRecvTimeout(unsigned int ms)
{
    DO_BOOL_FUNCTION(setRecvTimeout, ms)
}

bool MixedTcpUdtServerSocket::getRecvTimeout(unsigned int* millis) const
{
    DO_BOOL_FUNCTION(getRecvTimeout, millis)
}

bool MixedTcpUdtServerSocket::setSendTimeout(unsigned int ms)
{
    DO_BOOL_FUNCTION(setSendTimeout, ms)
}

bool MixedTcpUdtServerSocket::getSendTimeout(unsigned int* millis) const
{
    DO_BOOL_FUNCTION(getSendTimeout, millis)
}

bool MixedTcpUdtServerSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    for( AbstractStreamServerSocket* serverSock : m_socketDelegates )
    {
        if( !serverSock->getLastError( errorCode ) )
            return false;
        if( *errorCode != SystemError::noError )
            break;
    }
    return true;
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

    for( size_t i = 0; i < m_socketDelegates.size(); ++i )
    {
        if( m_acceptingFlags[i] )
            continue;
        using namespace std::placeholders;
        if( !m_socketDelegates[i]->acceptAsync(std::bind(&MixedTcpUdtServerSocket::connectionAcceptedSync, this, m_socketDelegates[i], i, _1, _2)) )
            return nullptr;
        m_acceptingFlags[i] = true;
    }

    {
        m_inSyncAccept.store( true, std::memory_order_relaxed );
        while( m_queue.empty() )
            m_cond.wait(&m_mutex);
        m_inSyncAccept.store( false, std::memory_order_relaxed );;
    }

    std::pair<SystemError::ErrorCode, AbstractStreamSocket*> conn = m_queue.front();
    m_queue.pop();
    if( conn.first )
        SystemError::setLastErrorCode(conn.first);
    return conn.second;
}

void MixedTcpUdtServerSocket::cancelAsyncIO(bool waitForRunningHandlerCompletion)
{
    for( size_t i = 0; i < m_socketDelegates.size(); ++i )
        m_socketDelegates[i]->cancelAsyncIO(waitForRunningHandlerCompletion);

    if( !m_inSyncAccept.load(std::memory_order_relaxed) )
        return;
    QMutexLocker lk(&m_mutex);
    m_queue.push(std::make_pair(SystemError::interrupted, nullptr));
    m_cond.wakeAll();
    for( size_t i = 0; i < m_socketDelegates.size(); ++i )
        m_acceptingFlags[i] = false;
}

void MixedTcpUdtServerSocket::terminateAsyncIO( bool waitForRunningHandlerCompletion )
{
    //TODO #ak
    assert( false );
}

bool MixedTcpUdtServerSocket::postImpl( std::function<void()>&& handler )
{
    return m_socketDelegates[0]->post( std::move(handler) );
}

bool MixedTcpUdtServerSocket::dispatchImpl( std::function<void()>&& handler )
{
    return m_socketDelegates[0]->dispatch( std::move(handler) );
}

bool MixedTcpUdtServerSocket::acceptAsyncImpl(std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>&& handler)
{
    for( size_t i = 0; i < m_socketDelegates.size(); ++i )
    {
        if( m_acceptingFlags[i] )
            continue;
        using namespace std::placeholders;
        if( !m_socketDelegates[i]->acceptAsync(std::bind(&MixedTcpUdtServerSocket::connectionAcceptedAsync, this, m_socketDelegates[i], i, _1, _2)) )
            return false;
        m_acceptingFlags[i] = true;
    }

    m_asyncHandler = std::move( handler );
    return true;
}

void MixedTcpUdtServerSocket::connectionAcceptedSync(
    AbstractStreamServerSocket* /*serverSocket*/,
    size_t serverSocketIndex,
    SystemError::ErrorCode errorCode,
    AbstractStreamSocket* newConnection )
{
    QMutexLocker lk(&m_mutex);
    m_queue.push(std::make_pair(errorCode, newConnection));
    m_cond.wakeAll();
    m_acceptingFlags[serverSocketIndex] = false;
}

void MixedTcpUdtServerSocket::connectionAcceptedAsync(
    AbstractStreamServerSocket* /*serverSocket*/,
    size_t serverSocketIndex,
    SystemError::ErrorCode errorCode,
    AbstractStreamSocket* newConnection )
{
    QMutexLocker lk(&m_mutex);

    m_acceptingFlags[serverSocketIndex] = false;
    assert( m_asyncHandler );   //assuming acceptAsync is always called from handler
    auto asyncHandler = std::move( m_asyncHandler );
    asyncHandler( errorCode, newConnection );
}
