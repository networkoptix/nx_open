#ifndef __UDT_SOCKET_H__
#define __UDT_SOCKET_H__
#include "abstract_socket.h"
#include "socket_common.h"
#include "system_socket.h"
#include "aio/pollset.h"
#include <memory>

#include "common_socket_impl.h"

class UdtSocket;
typedef CommonSocketImpl<UdtSocket> UDTSocketImpl;


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
class UdtPollSetImpl;
class UdtPollSetConstIteratorImpl;
// This is a work around for incomplete type within the unique_ptr. If this type is not
// in the namespace scope, such work around will not be needed since a non trivial ctor/dtor
// will be perfectly enough. However once in namespace scope, a wrapper pointer is needed
// to make C++ compiler happy. 

struct UdtSocketImplPtr : public std::unique_ptr<UdtSocketImpl> {
    UdtSocketImplPtr( UdtSocketImpl* impl );
    ~UdtSocketImplPtr();
};

struct UdtPollSetImplPtr : public std::unique_ptr<UdtPollSetImpl> {
    UdtPollSetImplPtr( UdtPollSetImpl* imp );
    ~UdtPollSetImplPtr();
};

struct UdtPollSetConstIteratorImplPtr : public std::unique_ptr<UdtPollSetConstIteratorImpl> {
    UdtPollSetConstIteratorImplPtr( UdtPollSetConstIteratorImpl* imp );
    UdtPollSetConstIteratorImplPtr();
    ~UdtPollSetConstIteratorImplPtr();
};

}// namespace detail

// Adding a level indirection to make C++ type system happy.
class UdtSocket {
public:
    UdtSocket();
    ~UdtSocket();

    bool getLastError(SystemError::ErrorCode* errorCode);
    bool getRecvTimeout(unsigned int* millis);
    bool getSendTimeout(unsigned int* millis);

    UDTSocketImpl* impl();
    const UDTSocketImpl* impl() const;

protected:
    UdtSocket( detail::UdtSocketImpl* impl );
    detail::UdtSocketImplPtr impl_;
    friend class UdtPollSet;
};

// BTW: Why some getter function has const qualifier, and others don't have this in AbstractStreamSocket ??

class UdtStreamSocket : public UdtSocket , public AbstractStreamSocket {
public:
    // AbstractSocket --------------- interface
    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual SocketAddress getPeerAddress() const override;
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
    virtual SocketAddress getForeignAddress() const override {
        return getPeerAddress();
    }
    virtual bool isConnected() const override;
    virtual void cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion = true ) override;
    //!Implementation of AbstractSocket::terminateAsyncIO
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;


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
    UdtStreamSocket();
    UdtStreamSocket( detail::UdtSocketImpl* impl );
    // We must declare this trivial constructor even it is trivial.
    // Since this will make std::unique_ptr call correct destructor for our
    // partial, forward declaration of class UdtSocketImp;
    virtual ~UdtStreamSocket();

protected:
    //!Implementation of AbstractSocket::postImpl
    virtual bool postImpl( std::function<void()>&& handler ) override;
    //!Implementation of AbstractSocket::dispatchImpl
    virtual bool dispatchImpl( std::function<void()>&& handler ) override;

private:
    std::unique_ptr<AsyncSocketImplHelper<UdtSocket>> m_aioHelper;

    virtual bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) override;
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    virtual bool registerTimerImpl( unsigned int timeoutMillis, std::function<void()>&& handler ) override;

private:
    Q_DISABLE_COPY(UdtStreamSocket)
};

class UdtStreamServerSocket : public UdtSocket, public AbstractStreamServerSocket  {
public:
    // AbstractStreamServerSocket -------------- interface
    virtual bool listen( int queueLen = 128 ) ;
    virtual AbstractStreamSocket* accept() ;
    virtual void cancelAsyncIO(bool waitForRunningHandlerCompletion) override;
    //!Implementation of AbstractSocket::terminateAsyncIO
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;

    virtual bool bind(const SocketAddress& localAddress);
    virtual SocketAddress getLocalAddress() const;
    virtual SocketAddress getPeerAddress() const;
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

    UdtStreamServerSocket();
    virtual ~UdtStreamServerSocket();

protected:
    //!Implementation of AbstractSocket::postImpl
    virtual bool postImpl( std::function<void()>&& handler ) override;
    //!Implementation of AbstractSocket::dispatchImpl
    virtual bool dispatchImpl( std::function<void()>&& handler ) override;
    virtual bool acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>&& handler ) ;

private:
    std::unique_ptr<AsyncServerSocketHelper<UdtSocket>> m_aioHelper;

    Q_DISABLE_COPY(UdtStreamServerSocket)
};

// Udt poller 
class UdtPollSet {
public:
    class const_iterator {
    public:
        const_iterator();
        const_iterator( const const_iterator& );
        const_iterator( detail::UdtPollSetImpl* impl , bool end );
        ~const_iterator();
        const_iterator& operator=( const const_iterator& );
        const_iterator operator++(int);   
        const_iterator& operator++();      
        UdtSocket* socket();
        const UdtSocket* socket() const;
        aio::EventType eventType() const;
        void* userData();

        bool operator==( const const_iterator& right ) const;
        bool operator!=( const const_iterator& right ) const;

    private:
        detail::UdtPollSetConstIteratorImplPtr impl_;
    };
public:
    UdtPollSet();
    ~UdtPollSet();
    bool isValid() const {
        return static_cast<bool>(impl_);
    }
    void interrupt();
    bool add( UdtSocket* sock, aio::EventType eventType, void* userData = NULL );
    void* remove( UdtSocket*  sock, aio::EventType eventType );
    void* getUserData( UdtSocket*  sock, aio::EventType eventType ) const;
    bool canAcceptSocket( UdtSocket* sock ) const { Q_UNUSED(sock); return true; }
    int poll( int millisToWait = aio::INFINITE_TIMEOUT );
    const_iterator begin() const;
    const_iterator end() const;
    size_t size() const;
    static unsigned int maxPollSetSize() {
        return std::numeric_limits<unsigned int>::max();
    }

private:
    detail::UdtPollSetImplPtr impl_;
    Q_DISABLE_COPY(UdtPollSet)
};

#endif // __UDT_SOCKET_H__
