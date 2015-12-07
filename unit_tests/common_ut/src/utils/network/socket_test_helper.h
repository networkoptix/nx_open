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
#include <nx/utils/thread/mutex.h>


//!Reads/writes random data to/from connection
class TestConnection
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
class RandomDataTcpServer
:
    public QnStoppable,
    public QnJoinable
{
public:
    RandomDataTcpServer( size_t bytesToSendThrough );
    virtual ~RandomDataTcpServer();

    virtual void pleaseStop() override;
    virtual void join() override;

    bool start();

    SocketAddress addressBeingListened() const;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    const size_t m_bytesToSendThrough;
    QnMutex m_mutex;
    std::list<std::shared_ptr<TestConnection>> m_acceptedConnections;

    void onNewConnection( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection );
    void onConnectionDone( TestConnection* connection );
};

//!Establishes numerous connections to specified address, reads all connections (ignoring data) and sends random data back
/*!
    \note This class is not thread-safe
*/
class ConnectionsGenerator
:
    public QnStoppable,
    public QnJoinable
{
public:
    ConnectionsGenerator(
        const SocketAddress& remoteAddress,
        size_t maxSimultaneousConnectionsCount,
        size_t bytesToSendThrough );
    virtual ~ConnectionsGenerator();

    virtual void pleaseStop() override;
    virtual void join() override;

    void enableErrorEmulation(int errorPercent);
    void start();

    size_t totalConnectionsEstablished() const;
    size_t totalBytesSent() const;
    size_t totalBytesReceived() const;

private:
    typedef std::list<std::unique_ptr<TestConnection>> ConnectionsContainer;

    const SocketAddress m_remoteAddress;
    size_t m_maxSimultaneousConnectionsCount;
    const size_t m_bytesToSendThrough;
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

    void onConnectionFinished( int id, ConnectionsContainer::iterator connectionIter );
};

#endif  //SOCKET_TEST_HELPER_H
