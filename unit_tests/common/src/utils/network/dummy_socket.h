/**********************************************************
* 26 feb 2015
* a.kolesnikov
***********************************************************/

#ifndef DUMMY_SOCKET_H
#define DUMMY_SOCKET_H

#include <utils/network/abstract_socket.h>


//!Base class for socket, reading/writing data from/to any source (e.g., file)
class DummySocket
:
    public AbstractStreamSocket
{
public:
    DummySocket();

    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual SocketAddress getPeerAddress() const override;
    virtual bool setReuseAddrFlag( bool reuseAddr ) override;
    virtual bool getReuseAddrFlag( bool* val ) override;
    virtual bool setNonBlockingMode( bool val ) override;
    virtual bool getNonBlockingMode( bool* val ) const override;
    virtual bool getMtu( unsigned int* mtuValue ) override;
    virtual bool setSendBufferSize( unsigned int buffSize ) override;
    virtual bool getSendBufferSize( unsigned int* buffSize ) override;
    virtual bool setRecvBufferSize( unsigned int buffSize ) override;
    virtual bool getRecvBufferSize( unsigned int* buffSize ) override;
    virtual bool setRecvTimeout( unsigned int millis ) override;
    virtual bool getRecvTimeout( unsigned int* millis ) override;
    virtual bool setSendTimeout( unsigned int ms ) override;
    virtual bool getSendTimeout( unsigned int* millis ) override;
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) override;
    virtual SOCKET_HANDLE handle() const override;
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual void cancelAsyncIO( aio::EventType eventType = aio::etNone, bool waitForRunningHandlerCompletion = true ) override;

    virtual bool reopen() override;
    virtual bool setNoDelay( bool value ) override;
    virtual bool getNoDelay( bool* value ) override;
    virtual bool toggleStatisticsCollection( bool val ) override;
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;

protected:
    virtual bool postImpl( std::function<void()>&& handler ) override;
    virtual bool dispatchImpl( std::function<void()>&& handler ) override;

    virtual bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) override;
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    virtual bool registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler ) override;

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
    BufferSocket( const std::basic_string<uint8_t>& data );

    virtual void close() override;
    virtual bool isClosed() const override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;
    virtual bool isConnected() const override;

private:
    const std::basic_string<uint8_t>& m_data;
};

#endif  //DUMMY_SOCKET_H
