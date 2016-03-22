/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "socket_test_helper.h"

#include <atomic>
#include <iostream>

#include <nx/utils/log/log.h>

#include <utils/common/cpp14.h>


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
    TestTrafficLimitType limitType,
    size_t trafficLimit)
:
    m_socket( std::move( socket ) ),
    m_limitType( limitType ),
    m_trafficLimit( trafficLimit ),
    m_connected( true ),
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
    TestTrafficLimitType limitType,
    size_t trafficLimit)
:
    m_socket( SocketFactory::createStreamSocket() ),
    m_limitType(limitType),
    m_trafficLimit( trafficLimit ),
    m_connected( false ),
    m_remoteAddress(
        remoteAddress.address == HostAddress::anyHost ? HostAddress::localhost : remoteAddress.address,
        remoteAddress.port ),
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

    {
        std::unique_lock<std::mutex> lk(mtx1);
        NX_ASSERT(terminatedSocketsIDs.emplace(m_id, m_accepted).second);
    }
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::~TestConnection. "<<m_id<<std::endl;
#endif

    --TestConnection_count;
}

void TestConnection::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_socket->pleaseStop(std::move(handler));
}

void TestConnection::pleaseStopSync()
{
    m_socket->pleaseStopSync();
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

const std::chrono::milliseconds kDefaultSendTimeout(17000);

void TestConnection::start()
{
    if( m_connected )
        return startIO();

    if (!m_socket->setNonBlockingMode(true) || 
        !m_socket->setSendTimeout(kDefaultSendTimeout.count()) ||
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

void TestConnection::setOnFinishedEventHandler(
    nx::utils::MoveOnlyFunc<void(int, TestConnection*, SystemError::ErrorCode)> handler)
{
    m_finishedEventHandler = std::move(handler);
}

void TestConnection::onConnected( int id, SystemError::ErrorCode errorCode )
{
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::onConnected. "<<id<<std::endl;
#endif

    if( errorCode != SystemError::noError )
    {
        if (!m_finishedEventHandler)
            return;
        auto handler = std::move( m_finishedEventHandler );
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

    if( (errorCode != SystemError::noError && errorCode != SystemError::timedOut) ||
        (errorCode == SystemError::noError && bytesRead == 0))  //connection closed by remote side
    {
        if (!m_finishedEventHandler)
            return;
        auto handler = std::move(m_finishedEventHandler);
        return handler( id, this, errorCode );
    }

    m_totalBytesReceived += bytesRead;
    m_readBuffer.clear();
    m_readBuffer.reserve( READ_BUF_SIZE );

    if (m_limitType == TestTrafficLimitType::incoming &&
        m_totalBytesReceived >= m_trafficLimit)
    {
        if (!m_finishedEventHandler)
            return;
        auto handler = std::move(m_finishedEventHandler);
        return handler(id, this, SystemError::getLastOSErrorCode());
    }

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

    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        if (!m_finishedEventHandler)
            return;
        auto handler = std::move( m_finishedEventHandler );
        return handler( id, this, errorCode );
    }

    m_totalBytesSent += bytesWritten;
    if (m_limitType == TestTrafficLimitType::outgoing &&
        m_totalBytesSent >= m_trafficLimit)
    {
        if (!m_finishedEventHandler)
            return;
        auto handler = std::move( m_finishedEventHandler );
        return handler(id, this, SystemError::getLastOSErrorCode());
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
RandomDataTcpServer::RandomDataTcpServer(
    TestTrafficLimitType limitType,
    size_t trafficLimit)
:
    m_limitType(limitType),
    m_trafficLimit(trafficLimit),
    m_totalConnectionsAccepted(0)
{
}

RandomDataTcpServer::~RandomDataTcpServer()
{
}

void RandomDataTcpServer::setServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> serverSock)
{
    m_serverSocket = std::move(serverSock);
}

void RandomDataTcpServer::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_serverSocket->pleaseStop(
        [this, handler = std::move(handler)]() mutable
        {
            QnMutexLocker lk(&m_mutex);
            auto acceptedConnections = std::move(m_acceptedConnections);
            lk.unlock();

            BarrierHandler completionHandlerInvoker(std::move(handler));
            for (auto& connection: acceptedConnections)
            {
                auto connectionPtr = connection.get();
                connectionPtr->pleaseStop(
                    [connection = std::move(connection),
                        handler = completionHandlerInvoker.fork()]() mutable
                    {
                        connection.reset();
                        handler();
                    });
            }
        });
}

bool RandomDataTcpServer::start()
{
    if (!m_serverSocket)
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
    AbstractStreamSocket* newConnection)
{
    using namespace std::placeholders;

    //ignoring errors for now
    if (errorCode == SystemError::noError)
    {
        auto testConnection = std::make_shared<TestConnection>(
            std::unique_ptr<AbstractStreamSocket>(newConnection),
            m_limitType,
            m_trafficLimit);
        testConnection->setOnFinishedEventHandler(
            std::bind(&RandomDataTcpServer::onConnectionDone, this, _2));
        NX_LOGX(lm("Accepted connection %1. local address %2")
            .arg(testConnection.get()).arg(testConnection->getLocalAddress().toString()),
            cl_logDEBUG1);
        QnMutexLocker lk(&m_mutex);
        testConnection->start();
        m_acceptedConnections.emplace_back(std::move(testConnection));
        ++m_totalConnectionsAccepted;
    }

    m_serverSocket->acceptAsync(
        std::bind(&RandomDataTcpServer::onNewConnection, this, _1, _2));
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
    TestTrafficLimitType limitType,
    size_t trafficLimit,
    size_t maxTotalConnections)
:
    m_remoteAddress( remoteAddress ),
    m_maxSimultaneousConnectionsCount( maxSimultaneousConnectionsCount ),
    m_limitType( limitType ),
    m_trafficLimit( trafficLimit ),
    m_maxTotalConnections( maxTotalConnections ),
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
}

void ConnectionsGenerator::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    std::unique_lock<std::mutex> lk( m_mutex );
    m_terminated = true;
    auto connections = std::move(m_connections);
    lk.unlock();
    BarrierHandler allConnectionsStoppedFuture(std::move(handler));
    for (auto& idAndConnection: connections)
    {
        auto connectionPtr = idAndConnection.second.get();
        connectionPtr->pleaseStop(
            [this,
                connection = std::move(idAndConnection.second),
                handler = allConnectionsStoppedFuture.fork()]() mutable
            {
                {
                    std::unique_lock<std::mutex> lk(m_mutex);
                    m_totalBytesSent += connection->totalBytesSent();
                    m_totalBytesReceived += connection->totalBytesReceived();
                }
                connection.reset();
                handler();
            });
    }
}

void ConnectionsGenerator::enableErrorEmulation(int errorPercent)
{
    //TODO #ak enabling this causes deadlock between aio threads due to blocking cancellation in ~TestConnection
    m_errorEmulationPercent = errorPercent;
}

void ConnectionsGenerator::setOnFinishedHandler(
    nx::utils::MoveOnlyFunc<void()> func)
{
    m_onFinishedHandler = std::move(func);
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

        std::unique_ptr<TestConnection> connection(
            new TestConnection(
                m_remoteAddress,
                m_limitType,
                m_trafficLimit));
        connection->setOnFinishedEventHandler(
            std::bind(&ConnectionsGenerator::onConnectionFinished, this,
                std::placeholders::_1, std::placeholders::_3));
        if (m_localAddress)
            connection->setLocalAddress(*m_localAddress);
        connection->start();
        ++m_totalConnectionsEstablished;
        m_connections.emplace(connection->id(), std::move(connection));
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

std::vector<SystemError::ErrorCode> ConnectionsGenerator::totalErrors() const
{
    return m_errors;
}

void ConnectionsGenerator::onConnectionFinished(
    int id,
    SystemError::ErrorCode code)
{
    if (code != SystemError::noError)
    {
        m_errors.push_back(code);
        if (m_errors.size() < 3)
        {
            std::cerr
                << "onConnectionFinished: "
                << SystemError::toString(code).toStdString()
                << std::endl;
        }
    }

    std::unique_lock<std::mutex> lk(m_mutex);

    {
        std::unique_lock<std::mutex> lk(mtx1);
        NX_ASSERT(terminatedSocketsIDs.find(id) == terminatedSocketsIDs.end());
    }

    auto connectionIter = m_connections.find(id);
    if (connectionIter != m_connections.end())
    {
        //connection might have been removed by pleaseStop
        m_totalBytesSent += connectionIter->second->totalBytesSent();
        m_totalBytesReceived += connectionIter->second->totalBytesReceived();
        m_connections.erase(connectionIter);
    }
    if (m_terminated)
        return;

    if (m_maxTotalConnections == kInfiniteConnectionCount ||   //no limit
        m_totalConnectionsEstablished < m_maxTotalConnections)
    {
        addNewConnections(&lk);
    }
    else if (m_connections.empty())
    {
        if (m_onFinishedHandler)
        {
            auto handler = std::move(m_onFinishedHandler);
            handler();
        }
    }
}

void ConnectionsGenerator::addNewConnections(
    std::unique_lock<std::mutex>* const /*lock*/)
{
    while (m_connections.size() < m_maxSimultaneousConnectionsCount)
    {
        auto connection = std::make_unique<TestConnection>(
            m_remoteAddress,
            m_limitType,
            m_trafficLimit);
        connection->setOnFinishedEventHandler(
            std::bind(&ConnectionsGenerator::onConnectionFinished, this,
                std::placeholders::_1, std::placeholders::_3));
        const bool emulatingError =
            m_errorEmulationPercent > 0 &&
            m_errorEmulationDistribution(m_randomEngine) < m_errorEmulationPercent;
        if (emulatingError)
        {
            const SystemError::ErrorCode osErrorCode = SystemError::getLastOSErrorCode();
            std::cerr << "Failed to start test connection. " << SystemError::toString(osErrorCode).toStdString() << std::endl;
            //if (!m_finishedConnectionsIDs.insert(m_connections.back()->id()).second)
            //    int x = 0;
            //ignoring error for now
            return;
        }

        connection->start();
        m_connections.emplace(connection->id(), std::move(connection));
        ++m_totalConnectionsEstablished;
    }
}

}   //test
}   //network
}   //nx
