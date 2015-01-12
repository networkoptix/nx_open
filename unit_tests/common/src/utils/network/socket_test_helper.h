/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_TEST_HELPER_H
#define SOCKET_TEST_HELPER_H

#if 0

#include <list>
#include <memory>
#include <mutex>

#include <utils/common/joinable.h>
#include <utils/common/stoppable.h>
#include <utils/network/socket.h>


class TestConnection
{
public:
    TestConnection( std::unique_ptr<AbstractStreamSocket> connection, size_t bytesToSendThrough );
    TestConnection( const SocketAddress& remoteAddress, size_t bytesToSendThrough );
    virtual ~TestConnection();

    void setDoneHandler( std::function<void(SystemError::ErrorCode)> handler );

    bool start();

private:
    std::unique_ptr<AbstractStreamSocket> m_connection;
    const size_t m_bytesToSendThrough;
    bool m_connected;
    SocketAddress m_remoteAddress;
    std::function<void(SystemError::ErrorCode)> m_handler;

    void onConnected( SystemError::ErrorCode );
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

    void onNewConnection( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection );
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

    bool start();

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

    void onConnectionFinished( ConnectionsContainer::iterator connectionIter );
};

#endif

#endif  //SOCKET_TEST_HELPER_H
