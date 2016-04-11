/**********************************************************
* 26 feb 2015
* a.kolesnikov
***********************************************************/

#ifndef DUMMY_SOCKET_H
#define DUMMY_SOCKET_H

#include <nx/network/abstract_socket.h>


namespace nx {
namespace network {

//!Base class for socket, reading/writing data from/to any source (e.g., file)
class DummySocket
:
    public AbstractStreamSocket
{
public:
    DummySocket();

    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool setReuseAddrFlag( bool reuseAddr ) override;
    virtual bool getReuseAddrFlag( bool* val ) const override;
    virtual bool setNonBlockingMode( bool val ) override;
    virtual bool getNonBlockingMode( bool* val ) const override;
    virtual bool getMtu( unsigned int* mtuValue ) const  override;
    virtual bool setSendBufferSize( unsigned int buffSize ) override;
    virtual bool getSendBufferSize( unsigned int* buffSize ) const override;
    virtual bool setRecvBufferSize( unsigned int buffSize ) override;
    virtual bool getRecvBufferSize( unsigned int* buffSize ) const override;
    virtual bool setRecvTimeout( unsigned int millis ) override;
    virtual bool getRecvTimeout( unsigned int* millis ) const override;
    virtual bool setSendTimeout( unsigned int ms ) override;
    virtual bool getSendTimeout( unsigned int* millis ) const override;
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const  override;
    virtual SOCKET_HANDLE handle() const override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual void cancelIOAsync(
        aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;
    virtual void cancelIOSync(aio::EventType eventType) override;

    virtual bool reopen() override;
    virtual bool setNoDelay( bool value ) override;
    virtual bool getNoDelay( bool* value ) const override;
    virtual bool toggleStatisticsCollection( bool val ) override;
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;
    virtual bool setKeepAlive( boost::optional< KeepAliveOptions > info ) override;
    virtual bool getKeepAlive( boost::optional< KeepAliveOptions >* result ) const override;

    virtual void post( nx::utils::MoveOnlyFunc<void()> handler ) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler ) override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler ) override;

    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;

    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;

    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler ) override;

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

private:
    SocketAddress m_localAddress;
    SocketAddress m_remotePeerAddress;
};

//!Reads data from buffer. Sent data is ignored
class BufferSocket
:
    public DummySocket
{
public:
    BufferSocket( const std::string& data );

    virtual bool close() override;
    virtual bool shutdown() override;
    virtual bool isClosed() const override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;
    virtual bool isConnected() const override;

private:
    const std::string& m_data;
    bool m_isOpened;
    size_t m_curPos;
};

}   //network
}   //nx

#endif  //DUMMY_SOCKET_H
