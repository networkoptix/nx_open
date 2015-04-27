/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "socket_test_helper.h"


////////////////////////////////////////////////////////////
//// class TestConnection
////////////////////////////////////////////////////////////
namespace
{
    const size_t READ_BUF_SIZE = 4*1024;
}

TestConnection::TestConnection(
    std::unique_ptr<AbstractStreamSocket> socket,
    size_t bytesToSendThrough,
    std::function<void(TestConnection*, SystemError::ErrorCode)> handler )
:
    m_socket( std::move( socket ) ),
    m_bytesToSendThrough( bytesToSendThrough ),
    m_connected( true ),
    m_handler( std::move(handler) ),
    m_terminated( false ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 )
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData.resize( READ_BUF_SIZE );
}

TestConnection::TestConnection(
    const SocketAddress& remoteAddress,
    size_t bytesToSendThrough,
    std::function<void(TestConnection*, SystemError::ErrorCode)> handler )
:
    m_socket( SocketFactory::createStreamSocket() ),
    m_bytesToSendThrough( bytesToSendThrough ),
    m_connected( false ),
    m_remoteAddress(
        remoteAddress.address == HostAddress::anyHost ? HostAddress::localhost : remoteAddress.address,
        remoteAddress.port ),
    m_handler( std::move(handler) ),
    m_terminated( false ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 )
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData.resize( READ_BUF_SIZE );
}

TestConnection::~TestConnection()
{
    if( m_socket )
        m_socket->terminateAsyncIO( true );
}

void TestConnection::pleaseStop()
{
    //TODO
}

bool TestConnection::start()
{
    if( m_connected )
        return startIO();

    return
        m_socket->setNonBlockingMode(true) &&
        m_socket->connectAsync(
            m_remoteAddress,
            std::bind(&TestConnection::onConnected, this, std::placeholders::_1) );
}

size_t TestConnection::totalBytesSent() const
{
    return m_totalBytesSent;
}

size_t TestConnection::totalBytesReceived() const
{
    return m_totalBytesReceived;
}

void TestConnection::onConnected( SystemError::ErrorCode errorCode )
{
    if( errorCode != SystemError::noError )
    {
        m_socket->cancelAsyncIO();
        return m_handler( this, errorCode );
    }

    if( !startIO() )
        m_handler( this, errorCode );
}

bool TestConnection::startIO()
{
    using namespace std::placeholders;

    std::unique_lock<std::mutex> lk( m_mutex );
    //TODO #ak we need mutex here because async socket API lacks way to start async read and write atomically.
        //Should note that aio::AIOService provides such functionality

    if( !m_socket->readSomeAsync(
            &m_readBuffer,
            std::bind(&TestConnection::onDataReceived, this, _1, _2) ) )
    {
        return false;
    }

    if( !m_socket->sendAsync(
            m_outData,
            std::bind(&TestConnection::onDataSent, this, _1, _2) ) )
    {
        m_terminated = true;
        m_socket->cancelAsyncIO();
        return false;
    }

    return true;
}

void TestConnection::onDataReceived( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        if( m_terminated )
            return;
    }

    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        m_socket->cancelAsyncIO();
        return m_handler( this, errorCode );
    }

    m_totalBytesReceived += bytesRead;
    m_readBuffer.clear();
    m_readBuffer.reserve( READ_BUF_SIZE );

    using namespace std::placeholders;
    if( !m_socket->readSomeAsync(
            &m_readBuffer,
            std::bind(&TestConnection::onDataReceived, this, _1, _2) ) )
    {
        m_socket->cancelAsyncIO();
        m_handler( this, SystemError::getLastOSErrorCode() );
    }
}

void TestConnection::onDataSent( SystemError::ErrorCode errorCode, size_t bytesWritten )
{
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        if( m_terminated )
            return;
    }

    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        m_socket->cancelAsyncIO();
        return m_handler( this, errorCode );
    }

    m_totalBytesSent += bytesWritten;
    if( m_totalBytesSent >= m_bytesToSendThrough )
    {
        m_socket->cancelAsyncIO();
        m_handler( this, SystemError::getLastOSErrorCode() );
        return;
    }

    using namespace std::placeholders;
    if( !m_socket->sendAsync(
            m_outData,
            std::bind(&TestConnection::onDataSent, this, _1, _2) ) )
    {
        m_socket->cancelAsyncIO();
        m_handler( this, SystemError::getLastOSErrorCode() );
    }
}



////////////////////////////////////////////////////////////
//// class RandomDataTcpServer
////////////////////////////////////////////////////////////
RandomDataTcpServer::RandomDataTcpServer( size_t bytesToSendThrough )
:
    m_bytesToSendThrough( bytesToSendThrough )
{
}

RandomDataTcpServer::~RandomDataTcpServer()
{
    pleaseStop();
    join();
}

void RandomDataTcpServer::pleaseStop()
{
}

void RandomDataTcpServer::join()
{
    if( m_serverSocket )
        m_serverSocket->terminateAsyncIO( true );
}

bool RandomDataTcpServer::start()
{
    m_serverSocket.reset( SocketFactory::createStreamServerSocket() );
    if( !m_serverSocket->bind(SocketAddress()) ||
        !m_serverSocket->listen() )
    {
        m_serverSocket.reset();
        return false;
    }
    if( !m_serverSocket->acceptAsync( std::bind(
            &RandomDataTcpServer::onNewConnection, this,
            std::placeholders::_1, std::placeholders::_2 ) ) )
    {
        m_serverSocket.reset();
        return false;
    }

    return true;
}

SocketAddress RandomDataTcpServer::addressBeingListened() const
{
    return m_serverSocket->getLocalAddress();
}

void RandomDataTcpServer::onNewConnection( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection )
{
    //ignoring errors for now
    if( errorCode == SystemError::noError )
    {
        std::unique_ptr<TestConnection> testConnection( new TestConnection(
            std::unique_ptr<AbstractStreamSocket>(newConnection),
            m_bytesToSendThrough,
            std::bind(&RandomDataTcpServer::onConnectionDone, this, std::placeholders::_1 ) ) );
        if( testConnection->start() )
            testConnection.release();
        //TODO #ak save connection somewhere
    }

    m_serverSocket->acceptAsync( std::bind(
        &RandomDataTcpServer::onNewConnection, this,
        std::placeholders::_1, std::placeholders::_2 ) );
}

void RandomDataTcpServer::onConnectionDone( TestConnection* /*connection*/ )
{
    //TODO
}



////////////////////////////////////////////////////////////
//// class ConnectionsGenerator
////////////////////////////////////////////////////////////
ConnectionsGenerator::ConnectionsGenerator(
    const SocketAddress& remoteAddress,
    size_t maxSimultaneousConnectionsCount,
    size_t bytesToSendThrough )
:
    m_remoteAddress( remoteAddress ),
    m_maxSimultaneousConnectionsCount( maxSimultaneousConnectionsCount ),
    m_bytesToSendThrough( bytesToSendThrough ),
    m_terminated( false ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 ),
    m_totalConnectionsEstablished( 0 )
{
}

ConnectionsGenerator::~ConnectionsGenerator()
{
    pleaseStop();
    join();
}

void ConnectionsGenerator::pleaseStop()
{
    std::unique_lock<std::mutex> lk( m_mutex );
    m_terminated = true;
}

void ConnectionsGenerator::join()
{
    std::unique_lock<std::mutex> lk( m_mutex );
    assert( m_terminated );
    while( !m_connections.empty() )
    {
        std::unique_ptr<TestConnection> connection;
        m_connections.front().swap( connection );
        lk.unlock();
        connection.reset();
        lk.lock();
        if( m_connections.empty() )
            break;
        if( !m_connections.front() )
            m_connections.pop_front();
    }
}

bool ConnectionsGenerator::start()
{
    for( size_t i = 0; i < m_maxSimultaneousConnectionsCount; ++i )
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        m_connections.push_back( std::unique_ptr<TestConnection>() );
        std::unique_ptr<TestConnection> connection( new TestConnection(
            m_remoteAddress,
            m_bytesToSendThrough,
            std::bind(&ConnectionsGenerator::onConnectionFinished, this, std::prev(m_connections.end())) ) );
        m_connections.back().swap( connection );
        if( !m_connections.back()->start() )
        {
            m_terminated = true;
            ConnectionsContainer connections;
            m_connections.swap( connections );
            lk.unlock();
            connections.clear();
            return false;
        }
        ++m_totalConnectionsEstablished;
    }

    return true;
}

size_t ConnectionsGenerator::totalConnectionsEstablished() const
{
    return m_totalConnectionsEstablished;
}

size_t ConnectionsGenerator::totalBytesSent() const
{
    return m_totalBytesSent;
}

size_t ConnectionsGenerator::totalBytesReceived() const
{
    return m_totalBytesReceived;
}

void ConnectionsGenerator::onConnectionFinished( ConnectionsContainer::iterator connectionIter )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    if( *connectionIter )
    {
        m_totalBytesSent += connectionIter->get()->totalBytesSent();
        m_totalBytesReceived += connectionIter->get()->totalBytesReceived();
    }
    m_connections.erase( connectionIter );
    if( m_terminated )
        return;

    while( m_connections.size() < m_maxSimultaneousConnectionsCount )
    {
        m_connections.push_back( std::unique_ptr<TestConnection>() );
        std::unique_ptr<TestConnection> connection( new TestConnection(
            m_remoteAddress,
            m_bytesToSendThrough,
            std::bind(&ConnectionsGenerator::onConnectionFinished, this, std::prev(m_connections.end())) ) );
        m_connections.back().swap( connection );
        if( !m_connections.back()->start() )
        {
            SystemError::ErrorCode osErrorCode = SystemError::getLastOSErrorCode();
            std::cerr<<"Failed to start test connection. "<<SystemError::toString(osErrorCode).toStdString()<<std::endl;
            //ignoring error for now
            m_connections.pop_back();
            return;
        }

        ++m_totalConnectionsEstablished;
    }
}
