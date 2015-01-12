/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "socket_test_helper.h"


#if 0
TestConnection::TestConnection( std::unique_ptr<AbstractStreamSocket> connection, size_t bytesToSendThrough )
:
    m_connection( std::move( connection ) ),
    m_bytesToSendThrough( bytesToSendThrough ),
    m_connected( true )
{
}

TestConnection::TestConnection( const SocketAddress& remoteAddress, size_t bytesToSendThrough )
:
    m_bytesToSendThrough( bytesToSendThrough ),
    m_connected( false ),
    m_remoteAddress( remoteAddress )
{
    m_connection.reset( SocketFactory::createStreamSocket() );
}

TestConnection::~TestConnection()
{
    if( m_connection )
        m_connection->terminateAsyncIO( true );
}

void TestConnection::setDoneHandler( std::function<void(SystemError::ErrorCode)> handler )
{
    m_handler = std::move(handler);
}

bool TestConnection::start()
{
    if( m_connected )
        return startIO();
    return m_connection->connectAsync(
        m_remoteAddress,
        std::bind(&TestConnection::onConnected, this, std::placeholders::_1) );
}

void TestConnection::onConnected( SystemError::ErrorCode errorCode )
{
    //TODO
}



RandomDataTcpServer::RandomDataTcpServer( size_t bytesToSendThrough )
:
    m_bytesToSendThrough( bytesToSendThrough )
{
}

RandomDataTcpServer::~RandomDataTcpServer()
{
    pelaseStop();
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
    if( !m_serverSocket->listen() )
    {
        m_serverSocket.reset();
        return false;
    }
    if( !serverSocket->acceptAsync( std::bind(
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
        std::unique_ptr<TestConnection> testConnection(
            new TestConnection(newConnection, m_bytesToSendThrough) );
        if( testConnection->start() )
            testConnection.release();
        //TODO #ak save connection somewhere
    }

    m_serverSocket->acceptAsync( std::bind(
        &RandomDataTcpServer::onNewConnection, this,
        std::placeholders::_1, std::placeholders::_2 ) );
}



ConnectionsGenerator::ConnectionsGenerator(
    const SocketAddress& remoteAddress,
    size_t maxSimultaneousConnectionsCount,
    size_t bytesToSendThrough )
:
    m_remoteAddress( remoteAddress ),
    m_maxSimultaneousConnectionsCount( maxSimultaneousConnectionsCount ),
    m_bytesToSendThrough( bytesToSendThrough ),
    m_terminated( false )
{
    pelaseStop();
    join();
}

ConnectionsGenerator::~ConnectionsGenerator()
{
    pleaseStop();
    join();
}

void ConnectionsGenerator::pleaseStop()
{
    //TODO
}

void ConnectionsGenerator::join()
{
    //TODO
}

bool ConnectionsGenerator::start()
{
    for( int i = 0; i < m_maxSimultaneousConnectionsCount; ++i )
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        std::unique_ptr<TestConnection> connection( new TestConnection( m_remoteAddress, m_bytesToSendThrough ) );
        m_connections.push_back( std::move(connection) );
        connection->setDoneHandler( std::bind(&ConnectionsGenerator::onConnectionFinished, this, m_connections.rbegin().base()) );
        if( !connection->start() )
        {
            m_terminated = true;
            ConnectionsContainer connections;
            m_connections.swap( connections );
            lk.unlock();
            connections.clear();
            return false;
        }
    }

    return true;
}

size_t ConnectionsGenerator::totalConnectionsEstablished() const
{
    //TODO
    return 0;
}

size_t ConnectionsGenerator::totalBytesSent() const
{
    //TODO
    return 0;
}

size_t ConnectionsGenerator::totalBytesReceived() const
{
    //TODO
    return 0;
}

void ConnectionsGenerator::onConnectionFinished( ConnectionsContainer::iterator connectionIter )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    m_connections.erase( connectionIter );
    if( m_terminated )
        return;

    std::unique_ptr<TestConnection> connection( new TestConnection( m_remoteAddress, m_bytesToSendThrough ) );
    m_connections.push_back( std::move(connection) );
    connection->setDoneHandler( std::bind(&ConnectionsGenerator::onConnectionFinished, this, m_connections.rbegin().base()) );
    assert( connection->start() );  //not processing error for now
}
#endif
