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

//!Reads/writes random data to/from connection
class NX_NETWORK_API TestConnection
:
    public QnStoppable
{
public:
    /*!
        \param handler to be called on connection closure or after \a bytesToSendThrough bytes have been sent
    */
    TestConnection(
        std::unique_ptr<AbstractStreamSocket> connection,
        size_t bytesToSendThrough,
        std::function<void(int, TestConnection*, SystemError::ErrorCode)> handler );
    /*!
        \param handler to be called on connection closure or after \a bytesToSendThrough bytes have been sent
    */
    TestConnection(
        const SocketAddress& remoteAddress,
        size_t bytesToSendThrough,
        std::function<void(int, TestConnection*, SystemError::ErrorCode)> handler );
    virtual ~TestConnection();

    virtual void pleaseStop() override;

    int id() const;
    void setLocalAddress(SocketAddress addr);
    SocketAddress getLocalAddress() const;
    void start();

    size_t totalBytesSent() const;
    size_t totalBytesReceived() const;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    const size_t m_bytesToSendThrough;
    bool m_connected;
    SocketAddress m_remoteAddress;
    const std::function<void(int, TestConnection*, SystemError::ErrorCode)> m_handler;
    nx::Buffer m_readBuffer;
    nx::Buffer m_outData;
    bool m_terminated;
    std::mutex m_mutex;
    size_t m_totalBytesSent;
    size_t m_totalBytesReceived;
    int m_id;
    boost::optional<SocketAddress> m_localAddress;
    bool m_accepted;

    void onConnected( int id, SystemError::ErrorCode );
    void startIO();
    void onDataReceived( int id, SystemError::ErrorCode errorCode, size_t bytesRead );
    void onDataSent( int id, SystemError::ErrorCode errorCode, size_t bytesWritten );

    TestConnection(const TestConnection&);
    TestConnection& operator=(const TestConnection&);
};

//!Server that listenes randome tcp-port, accepts connections, reads every connection and sends specified bytes number through every connection
/*!
    \note This class is not thread-safe
*/
class NX_NETWORK_API RandomDataTcpServer
:
    public QnStoppable,
    public QnJoinable
{
public:
    RandomDataTcpServer(size_t randomBytesToSendThrough);
    /** In this mode it sends \a dataToSend through connection and closes connection */
    RandomDataTcpServer(const QByteArray& dataToSend);
    virtual ~RandomDataTcpServer();

    void setServerSocket(std::unique_ptr<AbstractStreamServerSocket> serverSock);

    virtual void pleaseStop() override;
    virtual void join() override;

    void setLocalAddress(SocketAddress addr);
    bool start();

    SocketAddress addressBeingListened() const;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    const size_t m_bytesToSendThrough;
    QnMutex m_mutex;
    std::list<std::shared_ptr<TestConnection>> m_acceptedConnections;
    SocketAddress m_localAddress;

    void onNewConnection( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection );
    void onConnectionDone( TestConnection* connection );
};

//!Establishes numerous connections to specified address, reads all connections (ignoring data) and sends random data back
/*!
    \note This class is not thread-safe
*/
class NX_NETWORK_API ConnectionsGenerator
:
    public QnStoppable,
    public QnJoinable
{
public:
    /**
        @param maxTotalConnections If zero, then no limit on total connection number
    */
    ConnectionsGenerator(
        const SocketAddress& remoteAddress,
        size_t maxSimultaneousConnectionsCount,
        size_t bytesToSendThrough,
        size_t maxTotalConnections = 0);
    virtual ~ConnectionsGenerator();

    virtual void pleaseStop() override;
    virtual void join() override;

    void setOnFinishedHandler(nx::utils::MoveOnlyFunc<void()> func);
    void enableErrorEmulation(int errorPercent);
    void setLocalAddress(SocketAddress addr);
    void start();

    size_t totalConnectionsEstablished() const;
    size_t totalBytesSent() const;
    size_t totalBytesReceived() const;

private:
    typedef std::list<std::unique_ptr<TestConnection>> ConnectionsContainer;

    const SocketAddress m_remoteAddress;
    size_t m_maxSimultaneousConnectionsCount;
    const size_t m_bytesToSendThrough;
    const size_t m_maxTotalConnections;
    ConnectionsContainer m_connections;
    bool m_terminated;
    std::mutex m_mutex;
    size_t m_totalBytesSent;
    size_t m_totalBytesReceived;
    size_t m_totalConnectionsEstablished;
    std::set<int> m_finishedConnectionsIDs;
    std::random_device m_randomDevice;
    std::default_random_engine m_randomEngine;
    std::uniform_int_distribution<int> m_errorEmulationDistribution;
    int m_errorEmulationPercent;
    boost::optional<SocketAddress> m_localAddress;
    nx::utils::MoveOnlyFunc<void()> m_onFinishedHandler;

    void onConnectionFinished( int id, ConnectionsContainer::iterator connectionIter );
    void addNewConnections();
};

/** \class TCPSocket modification which randomly connects to different ports
 *  according to @param kShift */
template<quint16 kShift>
class MultipleClientSocketTester
    : public TCPSocket
{
public:
    MultipleClientSocketTester()
        : TCPSocket() {}

    bool connect(const SocketAddress& address, unsigned int timeout ) override
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
            m_address = SocketAddress(
                address.address, address.port + (modifier++ % kShift));

        return m_address;
    }

    SocketAddress m_address;
};

}   //test
}   //network
}   //nx

#endif  //SOCKET_TEST_HELPER_H
