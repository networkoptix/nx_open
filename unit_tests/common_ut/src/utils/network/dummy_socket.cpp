/**********************************************************
* 26 feb 2015
* a.kolesnikov
***********************************************************/

#include "dummy_socket.h"


DummySocket::DummySocket()
:
    m_localAddress( HostAddress::localhost, rand() )
{
}

bool DummySocket::bind( const SocketAddress& localAddress )
{
    m_localAddress = localAddress;
    return true;
}

SocketAddress DummySocket::getLocalAddress() const
{
    return m_localAddress;
}

bool DummySocket::setReuseAddrFlag( bool /*reuseAddr*/ )
{
    return true;
}

bool DummySocket::getReuseAddrFlag( bool* /*val*/ ) const
{
    return true;
}

bool DummySocket::setNonBlockingMode( bool /*val*/ )
{
    return true;
}

bool DummySocket::getNonBlockingMode( bool* /*val*/ ) const
{
    return true;
}

bool DummySocket::getMtu( unsigned int* mtuValue ) const
{
    *mtuValue = 1500;
    return true;
}

bool DummySocket::setSendBufferSize( unsigned int /*buffSize*/ )
{
    return true;
}

bool DummySocket::getSendBufferSize( unsigned int* buffSize ) const
{
    *buffSize = 8192;
    return true;
}

bool DummySocket::setRecvBufferSize( unsigned int /*buffSize*/ )
{
    return true;
}

bool DummySocket::getRecvBufferSize( unsigned int* buffSize ) const
{
    *buffSize = 8192;
    return true;
}

bool DummySocket::setRecvTimeout( unsigned int /*millis*/ )
{
    return true;
}

bool DummySocket::getRecvTimeout( unsigned int* millis ) const
{
    *millis = 0;
    return true;
}

bool DummySocket::setSendTimeout( unsigned int /*ms*/ )
{
    return true;
}

bool DummySocket::getSendTimeout( unsigned int* millis ) const
{
    *millis = 0;
    return true;
}

bool DummySocket::getLastError( SystemError::ErrorCode* /*errorCode*/ ) const
{
    return false;
}

AbstractSocket::SOCKET_HANDLE DummySocket::handle() const
{
    return 0;
}

void DummySocket::terminateAsyncIO( bool /*waitForRunningHandlerCompletion*/ )
{
}


bool DummySocket::connect(
    const SocketAddress& remoteSocketAddress,
    unsigned int /*timeoutMillis*/ )
{
    m_remotePeerAddress = remoteSocketAddress;
    return true;
}

SocketAddress DummySocket::getForeignAddress() const
{
    return m_remotePeerAddress;
}

void DummySocket::cancelAsyncIO( aio::EventType /*eventType*/,
                                 bool /*waitForRunningHandlerCompletion*/ )
{
}

bool DummySocket::reopen()
{
    return connect( m_remotePeerAddress );
}

bool DummySocket::setNoDelay( bool /*value*/ )
{
    return true;
}

bool DummySocket::getNoDelay( bool* /*value*/ ) const
{
    return true;
}

bool DummySocket::toggleStatisticsCollection( bool /*val*/ )
{
    return false;
}

bool DummySocket::getConnectionStatistics( StreamSocketInfo* /*info*/ )
{
    return false;
}

bool DummySocket::setKeepAlive( boost::optional< KeepAliveOptions > info )
{
    return true;
}

bool DummySocket::getKeepAlive( boost::optional< KeepAliveOptions >* result )
{
    return false;
}

void DummySocket::postImpl( std::function<void()>&& /*handler*/ )
{
}

void DummySocket::dispatchImpl( std::function<void()>&& /*handler*/ )
{
}

void DummySocket::connectAsyncImpl( const SocketAddress& /*addr*/,
                                    std::function<void( SystemError::ErrorCode )>&& /*handler*/ )
{
}

void DummySocket::recvAsyncImpl( nx::Buffer* const /*buf*/,
                                 std::function<void( SystemError::ErrorCode, size_t )>&& /*handler*/ )
{
}

void DummySocket::sendAsyncImpl( const nx::Buffer& /*buf*/,
                                 std::function<void( SystemError::ErrorCode, size_t )>&& /*handler*/ )
{
}

void DummySocket::registerTimerImpl( unsigned int /*timeoutMs*/,
                                     std::function<void()>&& /*handler*/ )
{
}



////////////////////////////////////////////////////////////
//// parse utils
////////////////////////////////////////////////////////////

BufferSocket::BufferSocket( const std::string& data )
:
    m_data( data ),
    m_isOpened( false ),
    m_curPos( 0 )
{
}

void BufferSocket::close()
{
    m_isOpened = false;
}

bool BufferSocket::isClosed() const
{
    return !m_isOpened;
}

bool BufferSocket::connect(
    const SocketAddress& remoteSocketAddress,
    unsigned int timeoutMillis )
{
    if( !DummySocket::connect( remoteSocketAddress, timeoutMillis ) )
        return false;

    m_isOpened = true;
    m_curPos = 0;
    return true;
}

int BufferSocket::recv( void* buffer, unsigned int bufferLen, int /*flags*/ )
{
    const size_t bytesToCopy = std::min<size_t>( bufferLen, m_data.size() - m_curPos );
    memcpy( buffer, m_data.data() + m_curPos, bytesToCopy );
    m_curPos += bytesToCopy;
    return bytesToCopy;
}

int BufferSocket::send( const void* /*buffer*/, unsigned int bufferLen )
{
    if( !m_isOpened )
        return -1;
    return bufferLen;
}

bool BufferSocket::isConnected() const
{
    return m_isOpened;
}
