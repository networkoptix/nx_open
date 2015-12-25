#ifndef __UDT_SOCKET_H__
#define __UDT_SOCKET_H__

#include <memory>

#include "../abstract_socket.h"
#include "../socket_common.h"
#include "../system_socket.h"
#include "../aio/pollset.h"


class UdtSocket;


template<class SocketType> class AsyncSocketImplHelper;
template<class SocketType> class AsyncServerSocketHelper;

class UdtPollSet;
class UdtStreamSocket;
class UdtStreamServerSocket;
// I put the implementator inside of detail namespace to avoid namespace pollution.
// The reason is that I see many of the class prefer using Implementator , maybe 
// we want binary compatible of our source code. Anyway, this is not a bad thing
// but some sacrifice on inline function.
namespace detail {
class UdtSocketImpl;
}// namespace detail

// Adding a level indirection to make C++ type system happy.
class NX_NETWORK_API UdtSocket {
public:
    UdtSocket();
    virtual ~UdtSocket();

    bool getLastError(SystemError::ErrorCode* errorCode);
    bool getRecvTimeout(unsigned int* millis);
    bool getSendTimeout(unsigned int* millis);

    CommonSocketImpl<UdtSocket>* impl();
    const CommonSocketImpl<UdtSocket>* impl() const;

protected:
    UdtSocket( detail::UdtSocketImpl* impl );
    detail::UdtSocketImpl* m_impl;
    friend class UdtPollSet;

    UdtSocket(const UdtSocket&);
    UdtSocket& operator=(const UdtSocket&);
};

// BTW: Why some getter function has const qualifier, and others don't have this in AbstractStreamSocket ??

class NX_NETWORK_API UdtStreamSocket : public UdtSocket , public AbstractStreamSocket {
public:
    UdtStreamSocket(bool natTraversal = true);
    UdtStreamSocket(detail::UdtSocketImpl* impl);
    // We must declare this trivial constructor even it is trivial.
    // Since this will make std::unique_ptr call correct destructor for our
    // partial, forward declaration of class UdtSocketImp;
    virtual ~UdtStreamSocket();

    // AbstractSocket --------------- interface
    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual void close() override;
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
    virtual bool setRecvTimeout( unsigned int millis ) override;
    virtual bool getRecvTimeout( unsigned int* millis ) const override;
    virtual bool setSendTimeout( unsigned int ms ) override;
    virtual bool getSendTimeout( unsigned int* millis ) const override;
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const override;
    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    // AbstractCommunicatingSocket ------- interface
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;
    //  What's difference between foreign address with peer address 
    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;
    //!Implementation of AbstractCommunicatingSocket::cancelAsyncIO
    virtual void cancelIOAsync(
        aio::EventType eventType,
        std::function<void()> cancellationDoneHandler) override;

    // AbstractStreamSocket ------ interface
    virtual bool reopen() override;
    virtual bool setNoDelay( bool value ) override {
        Q_UNUSED(value);
        return false;
    }
    virtual bool getNoDelay( bool* /*value*/ ) const override {
        return false;
    }
    virtual bool toggleStatisticsCollection( bool val ) override {
        Q_UNUSED(val);
        return false;
    }
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override {
        // Haven't found any way to get RTT sample from UDT
        Q_UNUSED(info);
        return false;
    }
    virtual bool setKeepAlive( boost::optional< KeepAliveOptions > info ) override {
        Q_UNUSED(info);
        return false; // not implemented yet
    }
    virtual bool getKeepAlive( boost::optional< KeepAliveOptions >* result ) override {
        Q_UNUSED(result);
        return false; // not implemented yet
    }
    virtual void connectAsync(
        const SocketAddress& addr,
        std::function<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void registerTimer(
        unsigned int timeoutMillis,
        std::function<void()> handler) override;

    //!Implementation of AbstractSocket::post
    virtual void post( std::function<void()> handler ) override;
    //!Implementation of AbstractSocket::dispatch
    virtual void dispatch( std::function<void()> handler ) override;

private:
    std::unique_ptr<AsyncSocketImplHelper<UdtSocket>> m_aioHelper;

private:
    Q_DISABLE_COPY(UdtStreamSocket)
};

class NX_NETWORK_API UdtStreamServerSocket
:
    public UdtSocket,
    public AbstractStreamServerSocket
{
public:
    UdtStreamServerSocket();
    virtual ~UdtStreamServerSocket();

    // AbstractStreamServerSocket -------------- interface
    virtual bool listen( int queueLen = 128 ) ;
    virtual AbstractStreamSocket* accept() ;
    virtual void pleaseStop( std::function< void() > handler ) override;

    virtual bool bind(const SocketAddress& localAddress);
    virtual SocketAddress getLocalAddress() const;
    virtual void close();
    virtual bool isClosed() const;
    virtual bool setReuseAddrFlag( bool reuseAddr );
    virtual bool getReuseAddrFlag( bool* val ) const;
    virtual bool setNonBlockingMode( bool val );
    virtual bool getNonBlockingMode( bool* val ) const;
    virtual bool getMtu( unsigned int* mtuValue ) const;
    virtual bool setSendBufferSize( unsigned int buffSize );
    virtual bool getSendBufferSize( unsigned int* buffSize ) const;
    virtual bool setRecvBufferSize( unsigned int buffSize );
    virtual bool getRecvBufferSize( unsigned int* buffSize ) const;
    virtual bool setRecvTimeout( unsigned int millis );
    virtual bool getRecvTimeout( unsigned int* millis ) const;
    virtual bool setSendTimeout( unsigned int ms ) ;
    virtual bool getSendTimeout( unsigned int* millis ) const;
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const;
    virtual AbstractSocket::SOCKET_HANDLE handle() const;

    //!Implementation of AbstractSocket::post
    virtual void post( std::function<void()> handler ) override;
    //!Implementation of AbstractSocket::dispatch
    virtual void dispatch( std::function<void()> handler ) override;
    virtual void acceptAsync( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )> handler ) ;

private:
    std::unique_ptr<AsyncServerSocketHelper<UdtSocket>> m_aioHelper;

    Q_DISABLE_COPY(UdtStreamServerSocket)
};

#endif // __UDT_SOCKET_H__
