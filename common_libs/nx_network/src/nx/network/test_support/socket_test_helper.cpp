/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "socket_test_helper.h"

#include <atomic>
#include <iostream>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>

#include <utils/common/cpp14.h>
#include <utils/common/string.h>


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
    size_t trafficLimit,
    TestTransmissionMode transmissionMode)
:
    m_socket( std::move( socket ) ),
    m_limitType( limitType ),
    m_trafficLimit( trafficLimit ),
    m_transmissionMode( transmissionMode ),
    m_connected( true ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 ),
    m_timeoutsInARow( 0 ),
    m_id( ++TestConnectionIDCounter ),
    m_accepted(true)
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData = nx::utils::generateRandomData( READ_BUF_SIZE );

    ++TestConnection_count;
}

TestConnection::TestConnection(
    const SocketAddress& remoteAddress,
    TestTrafficLimitType limitType,
    size_t trafficLimit,
    TestTransmissionMode transmissionMode)
:
    m_socket( SocketFactory::createStreamSocket() ),
    m_limitType(limitType),
    m_trafficLimit( trafficLimit ),
    m_transmissionMode( transmissionMode ),
    m_connected( false ),
    m_remoteAddress(
        remoteAddress.address == HostAddress::anyHost ? HostAddress::localhost : remoteAddress.address,
        remoteAddress.port ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 ),
    m_timeoutsInARow( 0 ),
    m_id( ++TestConnectionIDCounter ),
    m_accepted(false)
{
    m_readBuffer.reserve( READ_BUF_SIZE );
    m_outData = nx::utils::generateRandomData( READ_BUF_SIZE );

    ++TestConnection_count;
}

static std::mutex terminatedSocketsIDsMutex;
static std::map<int, bool> terminatedSocketsIDs;


TestConnection::~TestConnection()
{
    NX_LOGX(lm("accepted %1. Destroying...").arg(m_accepted), cl_logDEBUG2);

    {
        std::unique_lock<std::mutex> lk(terminatedSocketsIDsMutex);
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
const std::chrono::milliseconds kDefaultRecvTimeout(17000);
const size_t kDefaultMaxTimeoutsInARow(5);

void TestConnection::start()
{
    if( m_connected )
        return startIO();

    if (!m_socket->setNonBlockingMode(true) || 
        !m_socket->setSendTimeout(kDefaultSendTimeout.count()) ||
        !m_socket->setRecvTimeout(kDefaultRecvTimeout.count()) ||
        (m_localAddress && !m_socket->bind(*m_localAddress)))
    {
        return m_socket->post(std::bind(
            &TestConnection::onConnected, this,
            SystemError::getLastOSErrorCode()));
    }

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

bool TestConnection::isTaskComplete() const
{
    switch (m_limitType) {
        case TestTrafficLimitType::none:
            return true;
        case TestTrafficLimitType::incoming:
            return m_totalBytesReceived >= m_trafficLimit;
        case TestTrafficLimitType::outgoing:
            return m_totalBytesSent >= m_trafficLimit;
    }

    return false;
}

void TestConnection::setOnFinishedEventHandler(
    nx::utils::MoveOnlyFunc<void(int, TestConnection*, SystemError::ErrorCode)> handler)
{
    m_finishedEventHandler = std::move(handler);
}

void TestConnection::onConnected( SystemError::ErrorCode errorCode )
{
#ifdef DEBUG_OUTPUT
    std::cout<<"TestConnection::onConnected. "<<id<<std::endl;
#endif

    if( errorCode != SystemError::noError )
    {
        NX_LOGX(lm("accepted %1. Connect error: %2")
            .arg(m_accepted).arg(SystemError::toString(errorCode)), cl_logWARNING);

        return reportFinish( errorCode );
    }

    startIO();
}

void TestConnection::startIO()
{
    //TODO #ak we need mutex here because async socket API lacks way to start async read and write atomically.
        //Should note that aio::AIOService provides such functionality

    switch (m_transmissionMode)
    {
        case TestTransmissionMode::spam: return startSpamIO();
        case TestTransmissionMode::pong: return startEchoIO();
        case TestTransmissionMode::ping: return startEchoTestIO();
    };

    NX_ASSERT(false);
}


void TestConnection::startSpamIO()
{
    using namespace std::placeholders;
    m_socket->readSomeAsync(
        &m_readBuffer,
        std::bind(&TestConnection::onDataReceived, this, _1, _2));
    NX_LOGX(lm("accepted %1. Sending %2 bytes of data to %3")
        .arg(m_accepted).arg(m_outData.size())
        .arg(m_socket->getForeignAddress().toString()),
        cl_logDEBUG2);
    m_socket->sendAsync(
        m_outData,
        std::bind(&TestConnection::onDataSent, this, _1, _2));
}

void TestConnection::startEchoIO()
{
    readAllAsync(
        [this]()
        {
            // just send data back
            m_outData = std::move(m_readBuffer);
            sendAllAsync(
                [this]
                {
                    // start all over again
                    m_readBuffer.resize(0);
                    m_readBuffer.reserve(READ_BUF_SIZE);
                    startEchoIO();
                });
        });
}

void TestConnection::startEchoTestIO()
{
    sendAllAsync(
        [this]()
        {
            readAllAsync(
                [this]
                {
                    // if all ok start all over again
                    if (m_readBuffer == m_outData)
                    {
                        NX_LOGX(lm("Echo virified bytes: %1")
                            .arg(m_readBuffer.size()), cl_logDEBUG2);

                        m_readBuffer.resize(0);
                        return startEchoTestIO();
                    }

                    NX_LOGX(lm("Recieved data does not match sent"),
                        cl_logERROR);

                    reportFinish( SystemError::invalidData );
                });
        });
}

void TestConnection::readAllAsync( std::function<void()> handler )
{
    if (m_limitType == TestTrafficLimitType::incoming &&
        m_totalBytesReceived >= m_trafficLimit)
    {
        return reportFinish( SystemError::noError );
    }

    m_socket->readSomeAsync(
        &m_readBuffer,
        [this, handler = std::move(handler)](
            SystemError::ErrorCode code, size_t bytes)
        {
            m_totalBytesReceived += bytes;

            if (code == SystemError::timedOut)
            {
                if (++m_timeoutsInARow == kDefaultMaxTimeoutsInARow)
                    return reportFinish( code );

                return readAllAsync(std::move(handler));
            }

            m_timeoutsInARow = 0;
            if (code != SystemError::noError || bytes == 0)
                return reportFinish( code );

            if (static_cast<size_t>(m_readBuffer.size()) >= READ_BUF_SIZE)
                handler();
            else
                return readAllAsync(std::move(handler));
        });
}

void TestConnection::sendAllAsync( std::function<void()> handler )
{
    if (m_limitType == TestTrafficLimitType::outgoing &&
        m_totalBytesSent >= m_trafficLimit)
    {
        return reportFinish( SystemError::noError );
    }

    m_socket->sendAsync(
        m_outData,
        [this, handler = std::move(handler)](
            SystemError::ErrorCode code, size_t bytes)
        {
            m_totalBytesSent += bytes;

            if (code == SystemError::timedOut)
            {
                if (++m_timeoutsInARow == kDefaultMaxTimeoutsInARow)
                    return reportFinish( code );

                return readAllAsync(std::move(handler));
            }

            if (code != SystemError::noError || bytes == 0)
                return reportFinish( code );

            handler();
        });
}

void TestConnection::onDataReceived(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    if (errorCode == SystemError::noError)
    {
        NX_LOGX(lm("accepted %1. Received %2 bytes of data")
            .arg(m_accepted).arg(bytesRead), cl_logDEBUG2);
    }
    else
    {
        NX_LOGX(lm("accepted %1. Receive error: %2")
            .arg(m_accepted).arg(SystemError::toString(errorCode)), cl_logWARNING);
    }

    if( (errorCode != SystemError::noError && errorCode != SystemError::timedOut) ||
        (errorCode == SystemError::noError && bytesRead == 0))  //connection closed by remote side
    {
        return reportFinish( errorCode );
    }

    m_totalBytesReceived += bytesRead;
    m_readBuffer.clear();
    m_readBuffer.reserve( READ_BUF_SIZE );

    if (m_limitType == TestTrafficLimitType::incoming &&
        m_totalBytesReceived >= m_trafficLimit)
    {
        return reportFinish( errorCode );
    }

    using namespace std::placeholders;
    m_socket->readSomeAsync(
        &m_readBuffer,
        std::bind(&TestConnection::onDataReceived, this,  _1, _2) );
}

void TestConnection::onDataSent(
    SystemError::ErrorCode errorCode,
    size_t bytesWritten )
{
    if (errorCode != SystemError::noError && errorCode != SystemError::timedOut)
    {
        NX_LOGX(lm("accepted %1. Send error: %2")
            .arg(m_accepted).arg(SystemError::toString(errorCode)),
            cl_logWARNING);

        return reportFinish( errorCode );
    }

    m_totalBytesSent += bytesWritten;
    if (m_limitType == TestTrafficLimitType::outgoing &&
        m_totalBytesSent >= m_trafficLimit)
    {
        return reportFinish( errorCode );
    }

    using namespace std::placeholders;
    NX_LOGX(lm("accepted %1. Sending %2 bytes of data to %3")
        .arg(m_accepted).arg(m_outData.size())
        .arg(m_socket->getForeignAddress().toString()),
        cl_logDEBUG2);
    m_socket->sendAsync(
        m_outData,
        std::bind(&TestConnection::onDataSent, this, _1, _2) );
}

void TestConnection::reportFinish( SystemError::ErrorCode code )
{
    if (!m_finishedEventHandler)
        return;

    auto handler = std::move(m_finishedEventHandler);
    return handler(m_id, this, code);
}


////////////////////////////////////////////////////////////
//// class ConnectionTestStatistics
////////////////////////////////////////////////////////////

QString toString(const ConnectionTestStatistics& data)
{
    return lm("Connections online: %1, total: %2. Bytes in/out: %3/%4.")
        .arg(data.onlineConnections).arg(data.totalConnections)
        .arg(bytesToString(data.bytesReceived))
        .arg(bytesToString(data.bytesSent));
}

bool operator==(
    const ConnectionTestStatistics& left,
    const ConnectionTestStatistics& right)
{
    return 
        left.bytesReceived == right.bytesReceived &&
        left.bytesSent == right.bytesSent &&
        left.totalConnections == right.totalConnections &&
        left.onlineConnections == right.onlineConnections;
}

bool operator!=(
    const ConnectionTestStatistics& left,
    const ConnectionTestStatistics& right)
{
    return !(left == right);
}

ConnectionTestStatistics operator-(
    const ConnectionTestStatistics& left,
    const ConnectionTestStatistics& right)
{
    return ConnectionTestStatistics{
        left.bytesReceived - right.bytesReceived,
        left.bytesSent - right.bytesSent,
        left.totalConnections - right.totalConnections,
        left.onlineConnections - right.onlineConnections};
}


////////////////////////////////////////////////////////////
//// class RandomDataTcpServer
////////////////////////////////////////////////////////////
RandomDataTcpServer::RandomDataTcpServer(
    TestTrafficLimitType limitType,
    size_t trafficLimit,
    TestTransmissionMode transmissionMode)
:
    m_limitType(limitType),
    m_trafficLimit(trafficLimit),
    m_transmissionMode(transmissionMode),
    m_totalConnectionsAccepted(0),
    m_totalBytesReceivedByClosedConnections(0),
    m_totalBytesSentByClosedConnections(0)
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
            auto acceptedConnections = std::move(m_aliveConnections);
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

ConnectionTestStatistics RandomDataTcpServer::statistics() const
{
    QnMutexLocker lock(&m_mutex);

    uint64_t bytesReceivedByAliveConnections = 0;
    uint64_t bytesSentByAliveConnections = 0;
    for (const auto& connection : m_aliveConnections)
    {
        bytesReceivedByAliveConnections += connection->totalBytesReceived();
        bytesSentByAliveConnections += connection->totalBytesSent();
    }

    return ConnectionTestStatistics{
        m_totalBytesReceivedByClosedConnections + bytesReceivedByAliveConnections,
        m_totalBytesSentByClosedConnections + bytesSentByAliveConnections,
        m_totalConnectionsAccepted,
        m_aliveConnections.size()};
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
            m_trafficLimit,
            m_transmissionMode);
        testConnection->setOnFinishedEventHandler(
            std::bind(&RandomDataTcpServer::onConnectionDone, this, _2));
        NX_LOGX(lm("Accepted connection %1. local address %2")
            .arg(testConnection.get()).arg(testConnection->getLocalAddress().toString()),
            cl_logDEBUG1);
        QnMutexLocker lk(&m_mutex);
        testConnection->start();
        m_aliveConnections.emplace_back(std::move(testConnection));
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
        m_aliveConnections.begin(),
        m_aliveConnections.end(),
        [connection](const std::shared_ptr<TestConnection>& sharedConnection) {
            return sharedConnection.get() == connection;
        });
    m_totalBytesReceivedByClosedConnections += connection->totalBytesReceived();
    m_totalBytesSentByClosedConnections += connection->totalBytesSent();
    if (it != m_aliveConnections.end())
        m_aliveConnections.erase(it);
}



////////////////////////////////////////////////////////////
//// class ConnectionsGenerator
////////////////////////////////////////////////////////////
ConnectionsGenerator::ConnectionsGenerator(
    const SocketAddress& remoteAddress,
    size_t maxSimultaneousConnectionsCount,
    TestTrafficLimitType limitType,
    size_t trafficLimit,
    size_t maxTotalConnections,
    TestTransmissionMode transmissionMode)
:
    ConnectionsGenerator(
        std::vector<SocketAddress>(1, std::move(remoteAddress)),
        maxSimultaneousConnectionsCount,
        limitType,
        trafficLimit,
        maxTotalConnections,
        transmissionMode)
{
}

ConnectionsGenerator::ConnectionsGenerator(
    std::vector<SocketAddress> remoteAddresses,
    size_t maxSimultaneousConnectionsCount,
    TestTrafficLimitType limitType,
    size_t trafficLimit,
    size_t maxTotalConnections,
    TestTransmissionMode transmissionMode)
:
    m_remoteAddresses( remoteAddresses ),
    m_remoteAddressesIterator( m_remoteAddresses.begin() ),
    m_maxSimultaneousConnectionsCount( maxSimultaneousConnectionsCount ),
    m_limitType( limitType ),
    m_trafficLimit( trafficLimit ),
    m_maxTotalConnections( maxTotalConnections ),
    m_transmissionMode( transmissionMode ),
    m_terminated( false ),
    m_totalBytesSent( 0 ),
    m_totalBytesReceived( 0 ),
    m_totalIncompleteTasks( 0 ),
    m_totalConnectionsEstablished( 0 ),
    m_randomEngine(m_randomDevice()),
    m_errorEmulationDistribution(1, 100),
    m_errorEmulationPercent(0)
{
    NX_CRITICAL(m_remoteAddresses.size());
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
                    if (!connection->isTaskComplete())
                        ++m_totalIncompleteTasks;
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

void ConnectionsGenerator::resetRemoteAddresses(
    std::vector<SocketAddress> remoteAddresses)
{
    NX_CRITICAL(remoteAddresses.size());
    std::unique_lock<std::mutex> lk(m_mutex);
    m_remoteAddresses = std::move(remoteAddresses);
    m_remoteAddressesIterator = m_remoteAddresses.begin();
}

void ConnectionsGenerator::start()
{
    for( size_t i = 0; i < m_maxSimultaneousConnectionsCount; ++i )
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        std::unique_ptr<TestConnection> connection(
            new TestConnection(
                nextAddress(),
                m_limitType,
                m_trafficLimit,
                m_transmissionMode));
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

ConnectionTestStatistics ConnectionsGenerator::statistics() const
{
    std::lock_guard<std::mutex> lk(m_mutex);

    uint64_t bytesReceivedByAliveConnections = 0;
    uint64_t bytesSentByAliveConnections = 0;
    for (const auto& connection : m_connections)
    {
        bytesReceivedByAliveConnections += connection.second->totalBytesReceived();
        bytesSentByAliveConnections += connection.second->totalBytesSent();
    }

    return ConnectionTestStatistics{
        m_totalBytesReceived + bytesReceivedByAliveConnections,
        m_totalBytesSent + bytesSentByAliveConnections,
        m_totalConnectionsEstablished,
        m_connections.size() };
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

size_t ConnectionsGenerator::totalIncompleteTasks() const
{
    return m_totalIncompleteTasks;
}

const std::map<SystemError::ErrorCode, size_t>&
    ConnectionsGenerator::returnCodes() const
{
    return m_returnCodes;
}

const SocketAddress& ConnectionsGenerator::nextAddress()
{
    const auto current = m_remoteAddressesIterator;

    if (++m_remoteAddressesIterator == m_remoteAddresses.end())
        m_remoteAddressesIterator = m_remoteAddresses.begin();

    return *current;
}

void ConnectionsGenerator::onConnectionFinished(
    int id,
    SystemError::ErrorCode code)
{
    NX_LOGX(lm("Connection %1 has finished: %2")
        .arg(id).arg(SystemError::toString(code)), cl_logDEBUG1);

    std::unique_lock<std::mutex> lk(m_mutex);
    m_returnCodes.emplace(code, 0).first->second++;

    {
        std::unique_lock<std::mutex> lk(terminatedSocketsIDsMutex);
        NX_ASSERT(terminatedSocketsIDs.find(id) == terminatedSocketsIDs.end());
    }

    auto connectionIter = m_connections.find(id);
    if (connectionIter != m_connections.end())
    {
        //connection might have been removed by pleaseStop
        m_totalBytesSent += connectionIter->second->totalBytesSent();
        m_totalBytesReceived += connectionIter->second->totalBytesReceived();
        if (!connectionIter->second->isTaskComplete())
            ++m_totalIncompleteTasks;
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
            nextAddress(),
            m_limitType,
            m_trafficLimit,
            m_transmissionMode);
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

AddressBinder::AddressBinder()
:
    m_currentNumber(0)
{
}

SocketAddress AddressBinder::bind()
{
    QnMutexLocker lock(&m_mutex);
    SocketAddress key(QString(QLatin1String("a%1")).arg(m_currentNumber++));
    NX_CRITICAL(m_map.emplace(key, std::set<SocketAddress>()).second);
    return key;
}

void AddressBinder::add(const SocketAddress& key, SocketAddress address)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_map.find(key);
    NX_CRITICAL(it != m_map.end());
    NX_CRITICAL(it->second.insert(std::move(address)).second);
}

void AddressBinder::remove(const SocketAddress& key, const SocketAddress& address)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_map.find(key);
    NX_CRITICAL(it != m_map.end());
    NX_CRITICAL(it->second.erase(std::move(address)));
}

std::set<SocketAddress> AddressBinder::get(const SocketAddress& key) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_map.find(key);
    NX_CRITICAL(it != m_map.end());
    return it->second;
}

boost::optional<SocketAddress> AddressBinder::random(const SocketAddress& key) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_map.find(key);
    if (it == m_map.end() || it->second.size() == 0)
        return boost::none;

    return *std::next(it->second.begin(), rand() % it->second.size());
}

MultipleClientSocketTester::MultipleClientSocketTester(AddressBinder* addressBinder)
:
    TCPSocket(),
    m_addressBinder(addressBinder)
{
}

bool MultipleClientSocketTester::connect(
    const SocketAddress& address, unsigned int timeout)
{
    return TCPSocket::connect(modifyAddress(address), timeout);
}

void MultipleClientSocketTester::connectAsync(
    const SocketAddress& address,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    TCPSocket::connectAsync(modifyAddress(address), std::move(handler));
}

SocketAddress MultipleClientSocketTester::modifyAddress(const SocketAddress& address)
{
    static std::atomic<size_t> enumirator(0);
    if (m_address == SocketAddress())
    {
        auto addressOpt = m_addressBinder->random(address);
        NX_CRITICAL(addressOpt);

        m_address = std::move(*addressOpt);
        NX_LOGX(lm("using %2 instead of '%1'").arg(address.toString())
            .arg(m_address.toString()), cl_logDEBUG2);
    }

    return m_address;
}

}   //test
}   //network
}   //nx
