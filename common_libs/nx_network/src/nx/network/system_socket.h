#ifndef __PRACTICALSOCKET_INCLUDED__
#define __PRACTICALSOCKET_INCLUDED__

#include <string>
#include <exception>

#include <QtCore/QString>

#ifdef Q_OS_WIN
#   include <winsock2.h>
#   include <WS2tcpip.h>
#else
#   include <sys/socket.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

#include "abstract_socket.h"
#include "aio/pollable.h"
#include "nettools.h"
#include "socket_factory.h"
#include "utils/common/byte_array.h"
#include "utils/common/systemerror.h"


namespace nx {
namespace network {

typedef CommonSocketImpl PollableSystemSocketImpl;

namespace aio {
template<class SocketType> class BaseAsyncSocketImplHelper;
template<class SocketType> class AsyncSocketImplHelper;
} // namespace aio

#ifdef Q_OS_WIN
typedef int socklen_t;
#endif

struct NX_NETWORK_API SystemSocketAddress
{
    std::shared_ptr<const sockaddr> ptr;
    socklen_t size;

    SystemSocketAddress();
    SystemSocketAddress(SocketAddress address, int ipVersion);
    operator SocketAddress() const;
};

/**
 *   Base class representing basic communication endpoint
 */
template<typename InterfaceToImplement>
class Socket
:
    public InterfaceToImplement,
    public nx::network::Pollable
{
public:
    Socket(
        int type,
        int protocol,
        int ipVersion,
        PollableSystemSocketImpl* impl = nullptr );
    Socket(
        int sockDesc,
        int ipVersion,
        PollableSystemSocketImpl* impl = nullptr );

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;
    virtual ~Socket();

    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override;

    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    virtual bool getRecvTimeout(unsigned int* millis) const override;
    virtual bool getSendTimeout(unsigned int* millis) const override;
    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool shutdown() override;

    virtual bool isClosed() const override;
    virtual bool setReuseAddrFlag( bool reuseAddr ) override;
    virtual bool getReuseAddrFlag( bool* val ) const override;
    virtual bool setNonBlockingMode( bool val ) override;
    virtual bool getNonBlockingMode( bool* val ) const override;
    virtual bool getMtu( unsigned int* mtuValue ) const override;
    virtual bool setSendBufferSize( unsigned int buffSize ) override;
    virtual bool getSendBufferSize( unsigned int* buffSize ) const override;
    virtual bool setRecvBufferSize( unsigned int buffSize ) override;
    virtual bool getRecvBufferSize( unsigned int* buffSize ) const override;
    virtual bool setRecvTimeout( unsigned int ms ) override;
    virtual bool setSendTimeout( unsigned int ms ) override;

    virtual Pollable* pollable() override;
    virtual void post( nx::utils::MoveOnlyFunc<void()> handler ) override;
    virtual void dispatch( nx::utils::MoveOnlyFunc<void()> handler ) override;


    /**
     *   If WinSock, unload the WinSock DLLs; otherwise do nothing.  We ignore
     *   this in our sample client code but include it in the library for
     *   completeness.  If you are running on Windows and you are concerned
     *   about DLL resource consumption, call this after you are done with all
     *   Socket instances.  If you execute this on Windows while some instance of
     *   Socket exists, you are toast.  For portability of client code, this is
     *   an empty function on non-Windows platforms so you can always include it.
     *   @param buffer buffer to receive the data
     *   @param bufferLen maximum number of bytes to read into buffer
     *   @return number of bytes read, 0 for EOF, and -1 for error
     */
    static void cleanUp() ;

    /**
     *   Resolve the specified service for the specified protocol to the
     *   corresponding port number in host byte order
     *   @param service service to resolve (e.g., "http")
     *   @param protocol protocol of service to resolve.  Default is "tcp".
     */
    static unsigned short resolveService(
        const QString &service,
        const QString &protocol = QLatin1String("tcp"));

    bool createSocket( int type, int protocol );

protected:
    const int m_ipVersion;

private:
    bool m_nonBlockingMode;
};

/**
 *   Socket which is able to connect, send, and receive
 */
template<class InterfaceToImplement>
class CommunicatingSocket
:
    public Socket<InterfaceToImplement>
{
    typedef CommunicatingSocket<InterfaceToImplement> SelfType;

public:
    CommunicatingSocket(
        int type,
        int protocol,
        int ipVersion,
        PollableSystemSocketImpl* sockImpl = nullptr );

    CommunicatingSocket(
        int newConnSD,
        int ipVersion,
        PollableSystemSocketImpl* sockImpl = nullptr );

    virtual ~CommunicatingSocket();

    //!Implementation of AbstractCommunicatingSocket::connect
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = AbstractCommunicatingSocket::kDefaultTimeoutMillis) override;

    //!Implementation of AbstractCommunicatingSocket::recv
    virtual int recv( void* buffer, unsigned int bufferLen, int flags ) override;
    //!Implementation of AbstractCommunicatingSocket::send
    virtual int send( const void* buffer, unsigned int bufferLen ) override;
    //!Implementation of AbstractCommunicatingSocket::getForeignAddress
    virtual SocketAddress getForeignAddress() const override;
    //!Implementation of AbstractCommunicatingSocket::isConnected
    virtual bool isConnected() const override;
    //!Implementation of AbstractCommunicatingSocket::connectAsync
    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::readSomeAsync
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsync
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::registerTimer
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::cancelAsyncIO
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    virtual bool close() override;
    virtual bool shutdown() override;

protected:
    bool connectToIp(const SocketAddress& remoteAddress, unsigned int timeoutMillis);

    std::unique_ptr<aio::AsyncSocketImplHelper<SelfType>> m_aioHelper;
    bool m_connected;
};

/**
 *   TCP socket for communication with other TCP sockets
 */
class NX_NETWORK_API TCPSocket
:
    public CommunicatingSocket<AbstractStreamSocket>
{
    typedef CommunicatingSocket<AbstractStreamSocket> base_type;

public:
    /**
     * Construct a TCP socket with no connection.
     */
    TCPSocket(int ipVersion);
    virtual ~TCPSocket();

    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;
    TCPSocket(TCPSocket&&) = delete;
    TCPSocket& operator=(TCPSocket&&) = delete;


    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractStreamSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of AbstractStreamSocket::reopen
    virtual bool reopen() override;
    //!Implementation of AbstractStreamSocket::setNoDelay
    virtual bool setNoDelay( bool value ) override;
    //!Implementation of AbstractStreamSocket::getNoDelay
    virtual bool getNoDelay( bool* value ) const override;
    //!Implementation of AbstractStreamSocket::toggleStatisticsCollection
    virtual bool toggleStatisticsCollection( bool val ) override;
    //!Implementation of AbstractStreamSocket::getConnectionStatistics
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;
    //!Implementation of AbstractStreamSocket::setKeepAlive
    virtual bool setKeepAlive( boost::optional< KeepAliveOptions > info ) override;
    //!Implementation of AbstractStreamSocket::getKeepAlive
    virtual bool getKeepAlive( boost::optional< KeepAliveOptions >* result ) const override;

private:
    // Access for TCPServerSocket::accept() connection creation
    friend class TCPServerSocketPrivate;

    #if defined(Q_OS_WIN)
        KeepAliveOptions m_keepAlive;
    #endif

    /** Used by TCPServerSocket class. */
    TCPSocket(int newConnSD, int ipVersion);
};

/**
 *   TCP socket class for servers
 */
class NX_NETWORK_API TCPServerSocket
:
    public Socket<AbstractStreamServerSocket>
{
    typedef Socket<AbstractStreamServerSocket> base_type;

public:
    TCPServerSocket(int ipVersion);
    ~TCPServerSocket();

    TCPServerSocket(const TCPServerSocket&) = delete;
    TCPServerSocket& operator=(const TCPServerSocket&) = delete;
    TCPServerSocket(TCPServerSocket&&) = delete;
    TCPServerSocket& operator=(TCPServerSocket&&) = delete;

    /**
     *   Blocks until a new connection is established on this socket or error
     *   @return new connection socket
     */
    static int accept(int sockDesc);

    //!Implementation of AbstractStreamServerSocket::listen
    virtual bool listen(int queueLen = 128) override;
    //!Implementation of AbstractStreamServerSocket::accept
    virtual AbstractStreamSocket* accept() override;
    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop(nx::utils::MoveOnlyFunc< void() > handler) override;
    virtual void pleaseStopSync(bool assertIfCalledUnderLock = true) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler) override;
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync() override;

    AbstractStreamSocket* systemAccept();

private:
    bool setListen(int queueLen);
};

/**
  *   UDP socket class
  */
class NX_NETWORK_API UDPSocket
:
    public CommunicatingSocket<AbstractDatagramSocket>
{
    typedef CommunicatingSocket<AbstractDatagramSocket> base_type;

public:
    static const unsigned int MAX_PACKET_SIZE = 64*1024 - 24 - 8;   //maximum ip datagram size - ip header length - udp header length

    /**
     *   Construct a UDP socket
     */
    UDPSocket(int ipVersion = AF_INET);
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&&) = delete;
    UDPSocket& operator=(UDPSocket&&) = delete;

    /**
     *   Unset foreign address and port
     *   @return true if disassociation is successful
     */
    //void disconnect() ;

    /**
     *   Send the given buffer as a UDP datagram to the
     *   specified address/port
     *   @param buffer buffer to be written
     *   @param bufferLen number of bytes to write
     *   @param foreignAddress address (IP address or name) to send to
     *   @param foreignPort port number to send to
     *   @return true if send is successful
     *
     */
    bool sendTo(const void *buffer, int bufferLen);

    /**
     *   Set the multicast TTL
     *   @param multicastTTL multicast TTL
     */
    bool setMulticastTTL(unsigned char multicastTTL) ;

    //!Implementation of AbstractDatagramSocket::joinGroup
    virtual bool joinGroup( const QString &multicastGroup ) override;
    virtual bool joinGroup( const QString &multicastGroup, const QString& multicastIF ) override;

    //!Implementation of AbstractDatagramSocket::leaveGroup
    virtual bool leaveGroup( const QString &multicastGroup ) override;
    virtual bool leaveGroup( const QString &multicastGroup, const QString& multicastIF ) override;

    //!Implementation of AbstractCommunicatingSocket::send
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    //!Implementation of AbstractDatagramSocket::setDestAddr
    virtual bool setDestAddr( const SocketAddress& foreignEndpoint ) override;

    //!Implementation of AbstractDatagramSocket::sendTo
    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const SocketAddress& foreignEndpoint ) override;
    //!Implementation of AbstractCommunicatingSocket::sendToAsync
    virtual void sendToAsync(
        const nx::Buffer& buf,
        const SocketAddress& foreignAddress,
        std::function<void(SystemError::ErrorCode, SocketAddress, size_t)> completionHandler) override;
    //!Implementation of AbstractCommunicatingSocket::recv
    /*!
        Actually calls \a UDPSocket::recvFrom and saves datagram source address/port
    */
    virtual int recv( void* buffer, unsigned int bufferLen, int flags ) override;
    //!Implementation of AbstractDatagramSocket::recvFrom
    virtual int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        SocketAddress* const sourceAddress ) override;
    //!Implementation of AbstractDatagramSocket::recvFromAsync
    virtual void recvFromAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, SocketAddress, size_t)> handler) override;
    //!Implementation of AbstractDatagramSocket::lastDatagramSourceAddress
    virtual SocketAddress lastDatagramSourceAddress() const override;
    //!Implementation of AbstractDatagramSocket::hasData
    virtual bool hasData() const override;
    //!Implementation of AbstractDatagramSocket::setMulticastIF
    /**
    *   Set the multicast send interface
    *   @param multicastIF multicast interface for sending packets
    */
    virtual bool setMulticastIF( const QString& multicastIF ) override;

private:
    SystemSocketAddress m_destAddr;
    SocketAddress m_prevDatagramAddress;

    void setBroadcast();
    /*!
        \param sourcePort Port is returned in host byte order
    */
    int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        HostAddress* const sourceAddress,
        quint16* const sourcePort );
};

}   //network
}   //nx

#endif
