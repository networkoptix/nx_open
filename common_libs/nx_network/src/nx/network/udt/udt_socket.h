#ifndef __UDT_SOCKET_H__
#define __UDT_SOCKET_H__

#include <memory>

#include "../abstract_socket.h"
#include "../socket_common.h"
#include "../system_socket.h"
#include "../aio/pollset.h"


namespace nx {
namespace network {

class UdtSocket;

namespace aio {

template<class SocketType> class AsyncSocketImplHelper;
template<class SocketType> class AsyncServerSocketHelper;

}   //aio

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
class NX_NETWORK_API UdtSocket
:
    public Pollable
{
public:
    UdtSocket();
    virtual ~UdtSocket();

    /** Binds UDT socket to an existing UDP socket.
        \note This method can be called just after \a UdtSocket creation.
        \note if method have failed \a UdtSocket instance MUST be destroyed!
    */
    bool bindToUdpSocket(UDPSocket&& udpSocket);

    bool getLastError(SystemError::ErrorCode* errorCode);
    bool getRecvTimeout(unsigned int* millis);
    bool getSendTimeout(unsigned int* millis);

    //CommonSocketImpl<UdtSocket>* impl();
    //const CommonSocketImpl<UdtSocket>* impl() const;

    //nx::network::aio::AbstractAioThread* getAioThread();
    //void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);

protected:
    UdtSocket( detail::UdtSocketImpl* impl );
    detail::UdtSocketImpl* m_impl;
    friend class UdtPollSet;

    UdtSocket(const UdtSocket&);
    UdtSocket& operator=(const UdtSocket&);
};

// BTW: Why some getter function has const qualifier, and others don't have this in AbstractStreamSocket ??

class NX_NETWORK_API UdtStreamSocket
        : public UdtSocket
        , public AbstractStreamSocket
{
public:
    UdtStreamSocket();
    UdtStreamSocket(detail::UdtSocketImpl* impl);
    // We must declare this trivial constructor even it is trivial.
    // Since this will make std::unique_ptr call correct destructor for our
    // partial, forward declaration of class UdtSocketImp;
    virtual ~UdtStreamSocket();

    // AbstractSocket --------------- interface
    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual void close() override;
    virtual void shutdown() override;
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
    bool setRendezvous(bool val);

    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    virtual nx::network::aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

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
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    // AbstractStreamSocket ------ interface
    virtual bool reopen() override;
    virtual bool setNoDelay(bool value) override;
    virtual bool getNoDelay(bool* /*value*/) const override;
    virtual bool toggleStatisticsCollection(bool val) override;
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override;
    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > info) override;
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* result) const override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMillis,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    //!Implementation of AbstractSocket::post
    virtual void post( nx::utils::MoveOnlyFunc<void()> handler ) override;
    //!Implementation of AbstractSocket::dispatch
    virtual void dispatch( nx::utils::MoveOnlyFunc<void()> handler ) override;

private:
    std::unique_ptr<aio::AsyncSocketImplHelper<Pollable>> m_aioHelper;
    bool m_noDelay;

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

    virtual void pleaseStop(nx::utils::MoveOnlyFunc< void() > handler) override;

    virtual bool bind(const SocketAddress& localAddress);
    virtual SocketAddress getLocalAddress() const;
    virtual void close();
    virtual void shutdown();
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
    virtual nx::network::aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    //!Implementation of AbstractSocket::post
    virtual void post( nx::utils::MoveOnlyFunc<void()> handler ) override;
    //!Implementation of AbstractSocket::dispatch
    virtual void dispatch( nx::utils::MoveOnlyFunc<void()> handler ) override;

    // AbstractStreamServerSocket -------------- interface
    virtual bool listen(int queueLen = 128);
    virtual AbstractStreamSocket* accept();
    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler);
    //!Implementation of AbstractStreamServerSocket::cancelIOAsync
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    //!Implementation of AbstractStreamServerSocket::cancelIOSync
    virtual void cancelIOSync() override;

    /** This method is for use by \a AsyncServerSocketHelper only. It just calls system call \a accept */
    AbstractStreamSocket* systemAccept();

private:
    std::unique_ptr<aio::AsyncServerSocketHelper<UdtStreamServerSocket>> m_aioHelper;

    Q_DISABLE_COPY(UdtStreamServerSocket)
};

}   //network
}   //nx

#endif // __UDT_SOCKET_H__
