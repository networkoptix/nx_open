#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <random>
#include <set>

#include <nx/network/async_stoppable.h>
#include <nx/network/socket.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/result_counter.h>

namespace nx {
namespace network {
namespace test {

enum class TestTrafficLimitType
{
    none, // never quit
    incoming, // quits when sends over limit
    outgoing, // quits when receives over limit
};

// TODO: #mux Think about server mode auto detection
enum class TestTransmissionMode
{
    spam, // sends random data as fast as possible, receive always
    ping, // sends random data and verifies if it comes back
    pong, // reads 4K buffer, sends same buffer back, waits for futher data...
    receiveOnly,
};

QString NX_NETWORK_API toString(TestTrafficLimitType type);
QString NX_NETWORK_API toString(TestTransmissionMode type);

//!Reads/writes random data to/from connection
class NX_NETWORK_API TestConnection:
    public QnStoppableAsync
{
public:
    static constexpr std::chrono::milliseconds kDefaultRwTimeout = std::chrono::seconds(17);
    static constexpr size_t kReadBufferSize = 4 * 1024;

    /*!
        \param handler to be called on connection closure or after bytesToSendThrough bytes have been sent
    */
    TestConnection(
        std::unique_ptr<AbstractStreamSocket> connection,
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode);
    /*!
        \param handler to be called on connection closure or after bytesToSendThrough bytes have been sent
    */
    TestConnection(
        const SocketAddress& remoteAddress,
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode);

    virtual ~TestConnection();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    int id() const;
    void setLocalAddress(SocketAddress addr);
    SocketAddress getLocalAddress() const;
    void start(std::chrono::milliseconds rwTimeout = kDefaultRwTimeout);

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
    uint64_t m_dataSequence;
    uint64_t m_curStreamPos;
    uint64_t m_lastSequenceReceived;

    TestConnection(
        std::unique_ptr<AbstractStreamSocket> socket,
        const SocketAddress& remoteAddress,
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode,
        bool isConnected,
        bool isAccepted);

    void onConnected(SystemError::ErrorCode code);
    void startIO();
    void startSpamIO();
    void startEchoIO();
    void startEchoTestIO();
    void startReceiveOnlyTestIO();
    void onDataReceived(SystemError::ErrorCode errorCode, size_t bytesRead);
    void onDataSent(SystemError::ErrorCode errorCode, size_t bytesWritten);
    void readAllAsync(std::function<void()> handler);
    void sendAllAsync(std::function<void()> handler);
    void reportFinish(SystemError::ErrorCode code);
    void prepareConsequentDataToSend(QByteArray* buf);
    void verifyDataReceived(const QByteArray& buf, size_t bytesRead);

    TestConnection(const TestConnection&);
    TestConnection& operator=(const TestConnection&);
};

struct ConnectionTestStatistics
{
    uint64_t bytesReceived;
    uint64_t bytesSent;
    size_t totalConnections;
    size_t onlineConnections;
};

NX_NETWORK_API QString toString(const ConnectionTestStatistics& data);

NX_NETWORK_API bool operator==(
    const ConnectionTestStatistics& left,
    const ConnectionTestStatistics& right);
NX_NETWORK_API bool operator!=(
    const ConnectionTestStatistics& left,
    const ConnectionTestStatistics& right);
/** Substracts field by field */
NX_NETWORK_API ConnectionTestStatistics operator-(
    const ConnectionTestStatistics& left,
    const ConnectionTestStatistics& right);

class NX_NETWORK_API ConnectionPool
{
public:
    virtual ~ConnectionPool() {}

    virtual ConnectionTestStatistics statistics() const = 0;
};

//!Server that listenes randome tcp-port, accepts connections, reads every connection and sends specified bytes number through every connection
/*!
    NOTE: This class is not thread-safe
*/
class NX_NETWORK_API RandomDataTcpServer:
    public QnStoppableAsync,
    public ConnectionPool
{
public:
    RandomDataTcpServer(
        TestTrafficLimitType limitType,
        size_t trafficLimit,
        TestTransmissionMode transmissionMode,
        bool doNotBind = false);
    /** In this mode it sends dataToSend through connection and closes connection */
    RandomDataTcpServer(const QByteArray& dataToSend);
    virtual ~RandomDataTcpServer();

    void setServerSocket(std::unique_ptr<AbstractStreamServerSocket> serverSock);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    void setLocalAddress(SocketAddress addr);
    bool start(std::chrono::milliseconds rwTimeout = TestConnection::kDefaultRwTimeout);

    SocketAddress addressBeingListened() const;
    virtual ConnectionTestStatistics statistics() const override;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    const TestTrafficLimitType m_limitType;
    const size_t m_trafficLimit;
    const TestTransmissionMode m_transmissionMode;
    mutable QnMutex m_mutex;
    std::list<std::shared_ptr<TestConnection>> m_aliveConnections;
    SocketAddress m_localAddress;
    size_t m_totalConnectionsAccepted;
    uint64_t m_totalBytesReceivedByClosedConnections;
    uint64_t m_totalBytesSentByClosedConnections;
    std::chrono::milliseconds m_rwTimeout;
    bool m_doNotBind;

    void onNewConnection(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> newConnection);
    void onConnectionDone(TestConnection* connection);
};

//!Establishes numerous connections to specified address, reads all connections (ignoring data) and sends random data back
/*!
    NOTE: This class is not thread-safe
*/
class NX_NETWORK_API ConnectionsGenerator
:
    public QnStoppableAsync,
    public ConnectionPool
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

    ConnectionsGenerator(
        std::vector<SocketAddress> remoteAddresses,
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
    void resetRemoteAddresses(std::vector<SocketAddress> remoteAddress);
    void start(std::chrono::milliseconds rwTimeout = TestConnection::kDefaultRwTimeout);

    virtual ConnectionTestStatistics statistics() const override;

    size_t totalConnectionsEstablished() const;
    size_t totalBytesSent() const;
    size_t totalBytesReceived() const;
    size_t totalIncompleteTasks() const;
    const utils::ResultCounter<SystemError::ErrorCode>& results();

private:
    const SocketAddress& nextAddress();

    /** map<connection id, connection> */
    typedef std::map<int, std::unique_ptr<TestConnection>> ConnectionsContainer;

    std::vector<SocketAddress> m_remoteAddresses;
    std::vector<SocketAddress>::const_iterator m_remoteAddressesIterator;
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
    utils::ResultCounter<SystemError::ErrorCode> m_results;
    size_t m_totalConnectionsEstablished;
    std::set<int> m_finishedConnectionsIDs;
    std::random_device m_randomDevice;
    std::default_random_engine m_randomEngine;
    std::uniform_int_distribution<int> m_errorEmulationDistribution;
    int m_errorEmulationPercent;
    boost::optional<SocketAddress> m_localAddress;
    nx::utils::MoveOnlyFunc<void()> m_onFinishedHandler;
    std::chrono::milliseconds m_rwTimeout;

    void onConnectionFinished(
        int id,
        SystemError::ErrorCode code);

    void addNewConnections(std::unique_lock<std::mutex>* const /*lock*/);
};

class NX_NETWORK_API AddressBinder
{
public:
    AddressBinder();
    SocketAddress bind();
    void add(const SocketAddress& key, SocketAddress address);
    void remove(const SocketAddress& key, const SocketAddress& address);
    void remove(const SocketAddress& key);
    std::set<SocketAddress> get(const SocketAddress& key) const;
    boost::optional<SocketAddress> random(const SocketAddress& key) const;

    struct Manager
    {
        AddressBinder* const binder;
        const SocketAddress key;

        explicit Manager(AddressBinder* b): binder(b), key(b->bind()) {}
        Manager(AddressBinder* b, SocketAddress k): binder(b), key(std::move(k)) {}
        void wipe() { binder->remove(key); }

        void add(SocketAddress a) { binder->add(key, std::move(a)); }
        void remove(const SocketAddress& a) { binder->remove(key, a); }
    };

private:
    mutable QnMutex m_mutex;
    size_t m_currentNumber;
    std::map<SocketAddress, std::set<SocketAddress>> m_map;
};

/**
 * A TCPSocket modification which randomly connects to different ports according to @p kShift.
 */
class NX_NETWORK_API MultipleClientSocketTester:
    public TCPSocket
{
public:
    MultipleClientSocketTester(AddressBinder* addressBinder);
    bool connect(const SocketAddress& address, unsigned int timeout) override;
    void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

private:
    SocketAddress modifyAddress(const SocketAddress& address);

    AddressBinder* const m_addressBinder;
    SocketAddress m_address;
};

} // namespace test
} // namespace network
} // namespace nx
