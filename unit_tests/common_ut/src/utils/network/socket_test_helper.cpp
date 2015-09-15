/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "socket_test_helper.h"

#include <atomic>


////////////////////////////////////////////////////////////
//// class TestConnection
////////////////////////////////////////////////////////////
namespace
{
    const size_t READ_BUF_SIZE = 4*1024;
    std::atomic<int> TestConnection_count(0);
}

//#define DEBUG_OUTPUT

TestConnection::TestConnection(
    std::unique_ptr<AbstractStreamSocket> socket,
    size_t bytesToSendThrough,
    std::function<void(int, TestConnection*, SystemError::ErrorCode)> handler )
:
    m_socket( std::move( socket ) ),
    m_bytesToSendThrough( bytesToSendThrough ),
    m_connected( true ),
    m_handler( std::move(handler) ),
    m_terminated( false ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 ),
    m_id( ++TestConnection_count )
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData.resize( READ_BUF_SIZE );
}

TestConnection::TestConnection(
    const SocketAddress& remoteAddress,
    size_t bytesToSendThrough,
    std::function<void(int, TestConnection*, SystemError::ErrorCode)> handler )
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
    m_totalBytesReceived( 0 ),
    m_id( ++TestConnection_count )
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData.resize( READ_BUF_SIZE );
}

static std::mutex mtx1;
static std::map<int, bool> terminatedSocketsIDs;


TestConnection::~TestConnection()
{
    std::unique_ptr<AbstractStreamSocket> _socket;
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_terminated = true;
        _socket = std::move(m_socket);
    }
    if( _socket )
        _socket->terminateAsyncIO( true );

    {
        std::unique_lock<std::mutex> lk(mtx1);
        assert(terminatedSocketsIDs.emplace(m_id, _socket ? true : false).second);
    }
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::~TestConnection. "<<m_id<<std::endl;
#endif
}

int TestConnection::id() const
{
    return m_id;
}

void TestConnection::pleaseStop()
{
    //TODO
}

bool TestConnection::start()
{
    std::unique_lock<std::mutex> lk(m_mutex);

    if( m_connected )
        return startIO();

    return
        m_socket->setNonBlockingMode(true) &&
        m_socket->connectAsync(
            m_remoteAddress,
            std::bind(&TestConnection::onConnected, this, m_id, std::placeholders::_1) );
}

size_t TestConnection::totalBytesSent() const
{
    return m_totalBytesSent;
}

size_t TestConnection::totalBytesReceived() const
{
    return m_totalBytesReceived;
}

void TestConnection::onConnected( int id, SystemError::ErrorCode errorCode )
{
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::onConnected. "<<id<<std::endl;
#endif

    std::unique_lock<std::mutex> lk( m_mutex );

    if( m_terminated )
        return;

    if( errorCode != SystemError::noError )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move( m_handler );
        lk.unlock();
        return handler( id, this, errorCode );
    }

    if( !startIO() )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move( m_handler );
        lk.unlock();
        return handler( id, this, SystemError::getLastOSErrorCode() );
    }
}

bool TestConnection::startIO()
{
    using namespace std::placeholders;

    //TODO #ak we need mutex here because async socket API lacks way to start async read and write atomically.
        //Should note that aio::AIOService provides such functionality

    if( !m_socket->readSomeAsync(
            &m_readBuffer,
            std::bind(&TestConnection::onDataReceived, this, m_id, _1, _2) ) )
    {
        return false;
    }

    if( !m_socket->sendAsync(
            m_outData,
            std::bind(&TestConnection::onDataSent, this, m_id, _1, _2) ) )
    {
        return false;
    }

    return true;
}

void TestConnection::onDataReceived( int id, SystemError::ErrorCode errorCode, size_t bytesRead )
{
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::onDataReceived. "<<id<<std::endl;
#endif

    std::unique_lock<std::mutex> lk( m_mutex );
    if( m_terminated )
        return;
    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move(m_handler);
        lk.unlock();
        return handler( id, this, errorCode );
    }

    m_totalBytesReceived += bytesRead;
    m_readBuffer.clear();
    m_readBuffer.reserve( READ_BUF_SIZE );

    using namespace std::placeholders;
    if( !m_socket->readSomeAsync(
            &m_readBuffer,
            std::bind(&TestConnection::onDataReceived, this, m_id,  _1, _2) ) )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move( m_handler );
        lk.unlock();
        handler( id, this, SystemError::getLastOSErrorCode() );
    }
}

void TestConnection::onDataSent( int id, SystemError::ErrorCode errorCode, size_t bytesWritten )
{
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::onDataSent. "<<id<<std::endl;
#endif

    std::unique_lock<std::mutex> lk( m_mutex );
    if( m_terminated )
        return;
    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move( m_handler );
        lk.unlock();
        return handler( id, this, errorCode );
    }

    m_totalBytesSent += bytesWritten;
    if( m_totalBytesSent >= m_bytesToSendThrough )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move( m_handler );
        lk.unlock();
        handler( id, this, SystemError::getLastOSErrorCode() );
        return;
    }

    using namespace std::placeholders;
    if( !m_socket->sendAsync(
            m_outData,
            std::bind(&TestConnection::onDataSent, this, m_id, _1, _2) ) )
    {
        m_socket->terminateAsyncIO(true);
        auto handler = std::move( m_handler );
        lk.unlock();
        handler( id, this, SystemError::getLastOSErrorCode() );
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
    m_serverSocket = SocketFactory::createStreamServerSocket();
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
            std::bind(&RandomDataTcpServer::onConnectionDone, this, std::placeholders::_2 ) ) );
        if( testConnection->start() )
            testConnection.release();
        //TODO #ak save connection somewhere
    }

    m_serverSocket->acceptAsync( std::bind(
        &RandomDataTcpServer::onNewConnection, this,
        std::placeholders::_1, std::placeholders::_2 ) );
}

void RandomDataTcpServer::onConnectionDone( TestConnection* connection )
{
    delete connection;
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
        std::unique_ptr<TestConnection> connection = std::move(m_connections.front());
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
            std::bind(&ConnectionsGenerator::onConnectionFinished, this,
                      std::placeholders::_1, std::prev(m_connections.end())) ) );
        m_connections.back().swap( connection );
        if( !m_connections.back()->start() )
        {
            const SystemError::ErrorCode osErrorCode = SystemError::getLastOSErrorCode();
            std::cerr << "Failure initially starting test connection "<<i<<". " 
                << SystemError::toString(osErrorCode).toStdString() << std::endl;
            m_connections.pop_back();
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

void ConnectionsGenerator::onConnectionFinished(int id, ConnectionsContainer::iterator connectionIter)
{
    std::unique_lock<std::mutex> lk( m_mutex );

    {
        std::unique_lock<std::mutex> lk(mtx1);
        assert(terminatedSocketsIDs.find(id) == terminatedSocketsIDs.end());
    }

    //if( !m_finishedConnectionsIDs.insert( id ).second )
    //    int x = 0;
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
            std::bind(&ConnectionsGenerator::onConnectionFinished, this,
                      std::placeholders::_1, std::prev(m_connections.end())) ) );
        m_connections.back().swap( connection );
        if( !m_connections.back()->start() )
        {
            const SystemError::ErrorCode osErrorCode = SystemError::getLastOSErrorCode();
            std::cerr<<"Failed to start test connection. "<<SystemError::toString(osErrorCode).toStdString()<<std::endl;
            //if (!m_finishedConnectionsIDs.insert(m_connections.back()->id()).second)
            //    int x = 0;
            //ignoring error for now
            auto connection = std::move(m_connections.back());
            m_connections.pop_back();
            lk.unlock();
            return;
        }

        ++m_totalConnectionsEstablished;
    }
}
