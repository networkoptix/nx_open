/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "socket_test_helper.h"

#include <atomic>
#include <iostream>

#include <nx/utils/log/log.h>


namespace nx {
namespace network {
namespace test {

////////////////////////////////////////////////////////////
//// class TestConnection
////////////////////////////////////////////////////////////
namespace
{
    const size_t READ_BUF_SIZE = 4*1024;
    std::atomic<int> TestConnectionIDCounter(0);
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
    m_id( ++TestConnectionIDCounter ),
    m_accepted(true)
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData.resize( READ_BUF_SIZE );

    ++TestConnection_count;
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
    m_id( ++TestConnectionIDCounter ),
    m_accepted(false)
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData.resize( READ_BUF_SIZE );

    ++TestConnection_count;
}

static std::mutex mtx1;
static std::map<int, bool> terminatedSocketsIDs;


TestConnection::~TestConnection()
{
    NX_LOGX(lm("accepted %1. Destroying...").arg(m_accepted), cl_logDEBUG1);

    std::unique_ptr<AbstractStreamSocket> _socket;
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_terminated = true;
        _socket = std::move(m_socket);
    }
    if( _socket )
        _socket->pleaseStopSync();

    {
        std::unique_lock<std::mutex> lk(mtx1);
        NX_ASSERT(terminatedSocketsIDs.emplace(m_id, _socket ? true : false).second);
    }
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::~TestConnection. "<<m_id<<std::endl;
#endif

    --TestConnection_count;
}

void TestConnection::pleaseStop()
{
    //TODO #ak
}

int TestConnection::id() const
{
    return m_id;
}

void TestConnection::setLocalAddress(SocketAddress addr)
{
    m_localAddress = std::move(addr);
}

SocketAddress TestConnection::getLocalAddress() const
{
    return m_socket->getLocalAddress();
}

void TestConnection::start()
{
    std::unique_lock<std::mutex> lk(m_mutex);

    if( m_connected )
        return startIO();

    if (!m_socket->setNonBlockingMode(true) || 
        (m_localAddress && !m_socket->bind(*m_localAddress)))
    {
        return m_socket->post(std::bind(
            &TestConnection::onConnected,
            this,
            m_id,
            SystemError::getLastOSErrorCode()));
    }

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
        m_socket->pleaseStopSync();
        auto handler = std::move( m_handler );
        lk.unlock();
        return handler( id, this, errorCode );
    }

    startIO();
}

void TestConnection::startIO()
{
    using namespace std::placeholders;

    //TODO #ak we need mutex here because async socket API lacks way to start async read and write atomically.
        //Should note that aio::AIOService provides such functionality

    m_socket->readSomeAsync(
        &m_readBuffer,
        std::bind(&TestConnection::onDataReceived, this, m_id, _1, _2) );
    NX_LOGX(lm("accepted %1. Sending %2 bytes of data to %3")
        .arg(m_accepted).arg(m_outData.size())
        .arg(m_socket->getForeignAddress().toString()),
        cl_logDEBUG1);
    m_socket->sendAsync(
        m_outData,
        std::bind(&TestConnection::onDataSent, this, m_id, _1, _2) );
}

void TestConnection::onDataReceived(
    int id,
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    if (errorCode == SystemError::noError)
    {
        NX_LOGX(lm("accepted %1. Received %2 bytes of data")
            .arg(m_accepted).arg(bytesRead), cl_logDEBUG1);
    }
    else
    {
        NX_LOGX(lm("accepted %1. Receive error: %2")
            .arg(m_accepted).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
    }

    std::unique_lock<std::mutex> lk( m_mutex );
    if( m_terminated )
        return;
    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        m_socket->pleaseStopSync();
        auto handler = std::move(m_handler);
        lk.unlock();
        return handler( id, this, errorCode );
    }

    m_totalBytesReceived += bytesRead;
    m_readBuffer.clear();
    m_readBuffer.reserve( READ_BUF_SIZE );

    using namespace std::placeholders;
    m_socket->readSomeAsync(
        &m_readBuffer,
        std::bind(&TestConnection::onDataReceived, this, m_id,  _1, _2) );
}

void TestConnection::onDataSent( int id, SystemError::ErrorCode errorCode, size_t bytesWritten )
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("accepted %1. Send error: %2")
            .arg(m_accepted).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
    }

    std::unique_lock<std::mutex> lk( m_mutex );
    if( m_terminated )
        return;
    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        m_socket->pleaseStopSync();
        auto handler = std::move( m_handler );
        lk.unlock();
        return handler( id, this, errorCode );
    }

    m_totalBytesSent += bytesWritten;
    if( m_totalBytesSent >= m_bytesToSendThrough )
    {
        m_socket->pleaseStopSync();
        auto handler = std::move( m_handler );
        lk.unlock();
        handler( id, this, SystemError::getLastOSErrorCode() );
        return;
    }

    using namespace std::placeholders;
    NX_LOGX(lm("accepted %1. Sending %2 bytes of data to %3")
        .arg(m_accepted).arg(m_outData.size())
        .arg(m_socket->getForeignAddress().toString()),
        cl_logDEBUG1);
    m_socket->sendAsync(
        m_outData,
        std::bind(&TestConnection::onDataSent, this, m_id, _1, _2) );
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
        m_serverSocket->pleaseStopSync();

    QnMutexLocker lk(&m_mutex);
    auto acceptedConnections = std::move(m_acceptedConnections);
    lk.unlock();
    acceptedConnections.clear();
}

bool RandomDataTcpServer::start()
{
    m_serverSocket = SocketFactory::createStreamServerSocket();
    if( !m_serverSocket->bind(m_localAddress) ||
        !m_serverSocket->listen() )
    {
        m_serverSocket.reset();
        return false;
    }
    m_serverSocket->acceptAsync( std::bind(
        &RandomDataTcpServer::onNewConnection, this,
        std::placeholders::_1, std::placeholders::_2 ) );

    return true;
}

void RandomDataTcpServer::setLocalAddress(SocketAddress addr)
{
    m_localAddress = std::move(addr);
}

SocketAddress RandomDataTcpServer::addressBeingListened() const
{
    const auto localAddress = m_serverSocket->getLocalAddress();
    return 
        localAddress.address == HostAddress::anyHost
        ? SocketAddress(HostAddress::localhost, localAddress.port)
        : localAddress;
}

void RandomDataTcpServer::onNewConnection(
    SystemError::ErrorCode errorCode,
    AbstractStreamSocket* newConnection )
{
    //ignoring errors for now
    if( errorCode == SystemError::noError )
    {
        std::shared_ptr<TestConnection> testConnection( new TestConnection(
            std::unique_ptr<AbstractStreamSocket>(newConnection),
            m_bytesToSendThrough,
            std::bind(&RandomDataTcpServer::onConnectionDone, this, std::placeholders::_2 )));
        NX_LOGX(lm("Accepted connection %1. local address %2")
            .arg(testConnection.get()).arg(testConnection->getLocalAddress().toString()),
            cl_logDEBUG1);
        testConnection->start();
        QnMutexLocker lk(&m_mutex);
        m_acceptedConnections.emplace_back(std::move(testConnection));
    }

    m_serverSocket->acceptAsync( std::bind(
        &RandomDataTcpServer::onNewConnection, this,
        std::placeholders::_1, std::placeholders::_2 ) );
}

void RandomDataTcpServer::onConnectionDone(
    TestConnection* connection )
{
    QnMutexLocker lk(&m_mutex);
    auto it = std::find_if(
        m_acceptedConnections.begin(),
        m_acceptedConnections.end(),
        [connection](const std::shared_ptr<TestConnection>& sharedConnection) {
            return sharedConnection.get() == connection;
        });
    if (it != m_acceptedConnections.end())
        m_acceptedConnections.erase(it);
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
    m_totalConnectionsEstablished( 0 ),
    m_randomEngine(m_randomDevice()),
    m_errorEmulationDistribution(1, 100),
    m_errorEmulationPercent(0)
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
    NX_ASSERT( m_terminated );
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

void ConnectionsGenerator::enableErrorEmulation(int errorPercent)
{
    //TODO #ak enabling this causes deadlock between aio threads due to blocking cancellation in ~TestConnection
    m_errorEmulationPercent = errorPercent;
}

void ConnectionsGenerator::setLocalAddress(SocketAddress addr)
{
    m_localAddress = std::move(addr);
}

void ConnectionsGenerator::start()
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
        if (m_localAddress)
            m_connections.back()->setLocalAddress(*m_localAddress);
        m_connections.back()->start();
        ++m_totalConnectionsEstablished;
    }
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
        NX_ASSERT(terminatedSocketsIDs.find(id) == terminatedSocketsIDs.end());
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
        const bool emulatingError = 
            m_errorEmulationPercent > 0 &&
            m_errorEmulationDistribution(m_randomEngine) < m_errorEmulationPercent;
        if (emulatingError)
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

        m_connections.back()->start();
        ++m_totalConnectionsEstablished;
    }
}

}   //test
}   //network
}   //nx
