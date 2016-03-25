/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_TEST_HELPER_H
#define SOCKET_TEST_HELPER_H

#include <list>
#include <memory>
#include <mutex>
#include <random>
#include <set>

#include <utils/common/joinable.h>
#include <utils/common/stoppable.h>
#include <nx/network/socket.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/mutex.h>


namespace nx {
namespace network {
namespace test {

enum class TestTrafficLimitType
{
    none, // never quit
    incoming, // quits when sends over limit
    outgoing, // quits when recieves over limit
};

enum class TestTransmissionMode
{
    spam, // spams random data as fast as follible, recieves alweys
    echo, // reads and sends the same data as avaliable
    echoTest, // sends randome data and verifies if it comes back
};

//!Reads/writes random data to/from connection
class NX_NETWORK_API TestConnection
:
    public QnStoppableAsync
{
public:
    /*!
        \param handler to be called on connection closure or after \a bytesToSendThrough bytes have been sent
    */
    TestConnection(
        std::unique_ptr<AbstractStreamSocket> connection,
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode);
    /*!
        \param handler to be called on connection closure or after \a bytesToSendThrough bytes have been sent
    */
    TestConnection(
        const SocketAddress& remoteAddress,
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode);

    virtual ~TestConnection();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync() override;

    int id() const;
    void setLocalAddress(SocketAddress addr);
    SocketAddress getLocalAddress() const;
    void start();

    size_t totalBytesSent() const;
    size_t totalBytesReceived() const;
    bool isTaskComplete() const;

    void setOnFinishedEventHandler(
        nx::utils::MoveOnlyFunc<void(int, TestConnection*, SystemError::ErrorCode)> handler);

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    const TestTrafficLimitType m_limitType;
    const size_t m_trafficLimit;
    const TestTransmissionMode m_transmissionMode;
    bool m_connected;
    SocketAddress m_remoteAddress;
    nx::utils::MoveOnlyFunc<
        void(int, TestConnection*, SystemError::ErrorCode)
    > m_finishedEventHandler;
    nx::Buffer m_readBuffer;
    nx::Buffer m_outData;
    size_t m_totalBytesSent;
    size_t m_totalBytesReceived;
    size_t m_timeoutsInARow;
    int m_id;
    boost::optional<SocketAddress> m_localAddress;
    const bool m_accepted;

    void onConnected( SystemError::ErrorCode code );
    void startIO();
    void onDataReceived( SystemError::ErrorCode errorCode, size_t bytesRead );
    void onDataSent( SystemError::ErrorCode errorCode, size_t bytesWritten );
    void readAllAsync( std::function<void()> handler );
    void sendAllAsync( std::function<void()> handler );
    void reportFinish( SystemError::ErrorCode code );

    TestConnection(const TestConnection&);
    TestConnection& operator=(const TestConnection&);
};

//!Server that listenes randome tcp-port, accepts connections, reads every connection and sends specified bytes number through every connection
/*!
    \note This class is not thread-safe
*/
class NX_NETWORK_API RandomDataTcpServer
:
    public QnStoppableAsync
{
public:
    RandomDataTcpServer(
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode);
    /** In this mode it sends \a dataToSend through connection and closes connection */
    RandomDataTcpServer(const QByteArray& dataToSend);
    virtual ~RandomDataTcpServer();

    void setServerSocket(std::unique_ptr<AbstractStreamServerSocket> serverSock);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    void setLocalAddress(SocketAddress addr);
    bool start();

    SocketAddress addressBeingListened() const;
    QString statusLine() const;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    const TestTrafficLimitType m_limitType;
    const size_t m_trafficLimit;
    const TestTransmissionMode m_transmissionMode;
    mutable QnMutex m_mutex;
    std::list<std::shared_ptr<TestConnection>> m_acceptedConnections;
    SocketAddress m_localAddress;
    size_t m_totalConnectionsAccepted;

    void onNewConnection( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection );
    void onConnectionDone( TestConnection* connection );
};

//!Establishes numerous connections to specified address, reads all connections (ignoring data) and sends random data back
/*!
    \note This class is not thread-safe
*/
class NX_NETWORK_API ConnectionsGenerator
:
    public QnStoppableAsync
{
public:
    static const size_t kInfiniteConnectionCount = 0;

    /**
        @param maxTotalConnections If zero, then no limit on total connection number
    */
    ConnectionsGenerator(
        const SocketAddress& remoteAddress,
        size_t maxSimultaneousConnectionsCount,
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        size_t maxTotalConnections,
        TestTransmissionMode transmissionMode);

    virtual ~ConnectionsGenerator();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    void setOnFinishedHandler(nx::utils::MoveOnlyFunc<void()> func);
    void enableErrorEmulation(int errorPercent);
    void setLocalAddress(SocketAddress addr);
    void start();

    size_t totalConnectionsEstablished() const;
    size_t totalBytesSent() const;
    size_t totalBytesReceived() const;
    size_t totalIncompleteTasks() const;
    QString returnCodes() const;

private:
    /** map<connection id, connection> */
    typedef std::map<int, std::unique_ptr<TestConnection>> ConnectionsContainer;

    const SocketAddress m_remoteAddress;
    size_t m_maxSimultaneousConnectionsCount;
    const TestTrafficLimitType m_limitType;
    const size_t m_trafficLimit;
    const size_t m_maxTotalConnections;
    const TestTransmissionMode m_transmissionMode;
    ConnectionsContainer m_connections;
    bool m_terminated;
    mutable std::mutex m_mutex;
    size_t m_totalBytesSent;
    size_t m_totalBytesReceived;
    size_t m_totalIncompleteTasks;
    std::map<SystemError::ErrorCode, size_t> m_returnCodes;
    size_t m_totalConnectionsEstablished;
    std::set<int> m_finishedConnectionsIDs;
    std::random_device m_randomDevice;
    std::default_random_engine m_randomEngine;
    std::uniform_int_distribution<int> m_errorEmulationDistribution;
    int m_errorEmulationPercent;
    boost::optional<SocketAddress> m_localAddress;
    nx::utils::MoveOnlyFunc<void()> m_onFinishedHandler;

    void onConnectionFinished(
        int id,
        SystemError::ErrorCode code);

    void addNewConnections(std::unique_lock<std::mutex>* const /*lock*/);
};

/**
 * A TCPSocket modification which randomly connects to different ports according to @p kShift.
 */
template<quint16 kShift>
class MultipleClientSocketTester
:
    public TCPSocket
{
public:
    MultipleClientSocketTester()
    :
        TCPSocket()
    {
    }

    bool connect(const SocketAddress& address, unsigned int timeout) override
    {
        return TCPSocket::connect(modifyAddress(address), timeout);
    }

    void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        TCPSocket::connectAsync(modifyAddress(address), std::move(handler));
    }

private:
    SocketAddress modifyAddress(const SocketAddress& address)
    {
        static quint16 modifier = 0;
        if (m_address == SocketAddress())
        {
            m_address = SocketAddress(
                address.address, address.port + (modifier++ % kShift));
        }

        return m_address;
    }

    SocketAddress m_address;
};

} // namespace test
} // namespace network
} // namespace nx

#endif  //SOCKET_TEST_HELPER_H
