/**********************************************************
* 26 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_IMPL_HELPER_H
#define SOCKET_IMPL_HELPER_H

#include "abstract_socket.h"


/*!
    \param AbstractSocketProviderType functor with \a operator() that returns type that implements function from \a AbstractSocket abstract class
*/
template<class AbstractBaseType, class AbstractSocketProviderType>
class AbstractSocketImplementationDelegate
:
    public AbstractBaseType
{
public:
    AbstractSocketImplementationDelegate(AbstractSocketProviderType abstractSocketProvider)
    :
        m_abstractSocketProvider(abstractSocketProvider)
    {
    }

    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of AbstractSocket::bind
    virtual bool bind(const SocketAddress& localAddress) override { return m_abstractSocketProvider()->bind(localAddress); }
    //!Implementation of AbstractSocket::getLocalAddress
    virtual SocketAddress getLocalAddress() const override { return m_abstractSocketProvider()->getLocalAddress(); }
    //!Implementation of AbstractSocket::close
    virtual void close() override { return m_abstractSocketProvider()->close(); }
    //!Implementation of AbstractSocket::shutdown
    virtual void shutdown() override { return m_abstractSocketProvider()->shutdown(); }
    //!Implementation of AbstractSocket::isClosed
    virtual bool isClosed() const override { return m_abstractSocketProvider()->isClosed(); }
    //!Implementation of AbstractSocket::setReuseAddrFlag
    virtual bool setReuseAddrFlag(bool reuseAddr) override { return m_abstractSocketProvider()->setReuseAddrFlag(reuseAddr); }
    //!Implementation of AbstractSocket::reuseAddrFlag
    virtual bool getReuseAddrFlag(bool* val) const override { return m_abstractSocketProvider()->getReuseAddrFlag(val); }
    //!Implementation of AbstractSocket::setNonBlockingMode
    virtual bool setNonBlockingMode(bool val) override { return m_abstractSocketProvider()->setNonBlockingMode(val); }
    //!Implementation of AbstractSocket::getNonBlockingMode
    virtual bool getNonBlockingMode(bool* val) const override { return m_abstractSocketProvider()->getNonBlockingMode(val); }
    //!Implementation of AbstractSocket::getMtu
    virtual bool getMtu(unsigned int* mtuValue) const override { return m_abstractSocketProvider()->getMtu(mtuValue); }
    //!Implementation of AbstractSocket::setSendBufferSize
    virtual bool setSendBufferSize(unsigned int buffSize) override { return m_abstractSocketProvider()->setSendBufferSize(buffSize); }
    //!Implementation of AbstractSocket::getSendBufferSize
    virtual bool getSendBufferSize(unsigned int* buffSize) const override { return m_abstractSocketProvider()->getSendBufferSize(buffSize); }
    //!Implementation of AbstractSocket::setRecvBufferSize
    virtual bool setRecvBufferSize(unsigned int buffSize) override { return m_abstractSocketProvider()->setRecvBufferSize(buffSize); }
    //!Implementation of AbstractSocket::getRecvBufferSize
    virtual bool getRecvBufferSize(unsigned int* buffSize) const override { return m_abstractSocketProvider()->getRecvBufferSize(buffSize); }
    //!Implementation of AbstractSocket::setRecvTimeout
    virtual bool setRecvTimeout(unsigned int ms) override { return m_abstractSocketProvider()->setRecvTimeout(ms); }
    //!Implementation of AbstractSocket::getRecvTimeout
    virtual bool getRecvTimeout(unsigned int* millis) const override { return m_abstractSocketProvider()->getRecvTimeout(millis); }
    //!Implementation of AbstractSocket::setSendTimeout
    virtual bool setSendTimeout(unsigned int ms) override { return m_abstractSocketProvider()->setSendTimeout(ms); }
    //!Implementation of AbstractSocket::getSendTimeout
    virtual bool getSendTimeout(unsigned int* millis) const override { return m_abstractSocketProvider()->getSendTimeout(millis); }
    //!Implementation of AbstractSocket::getLastError
    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override { return m_abstractSocketProvider()->getLastError(errorCode); }
    //!Implementation of AbstractSocket::handle
    virtual AbstractSocket::SOCKET_HANDLE handle() const override { return m_abstractSocketProvider()->handle(); }
    //!Implementation of AbstractSocket::post
    virtual void post( std::function<void()> handler ) override { m_abstractSocketProvider()->post( std::move(handler) ); }
    //!Implementation of AbstractSocket::dispatch
    virtual void dispatch( std::function<void()> handler ) override { m_abstractSocketProvider()->dispatch( std::move(handler) ); }
    //!Implementation of AbstractSocket::getAioThread
    virtual aio::AbstractAioThread* getAioThread() override { return m_abstractSocketProvider()->getAioThread(); }
    //!Implementation of AbstractSocket::bindToAioThread
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override { m_abstractSocketProvider()->bindToAioThread( aioThread ); }

private:
    AbstractSocketProviderType m_abstractSocketProvider;
};


//!Implements pure virtual methods of \a AbstractSocket by delegating them to embedded object of type \a AbstractSocketMethodsImplementorType
template<typename AbstractBaseType, typename AbstractSocketImplementorType>
class AbstractSocketImplementationEmbeddingDelegate
:
    public AbstractSocketImplementationDelegate<AbstractBaseType, std::function<AbstractSocketImplementorType*()>>
{
    typedef AbstractSocketImplementationDelegate<AbstractBaseType, std::function<AbstractSocketImplementorType*()>> base_type;

public:
    //TODO #ak replace multiple constructors with variadic template after move to msvc2013
    template<class Param1Type>
    AbstractSocketImplementationEmbeddingDelegate(const Param1Type& param1)
    :
        base_type([this](){ return &m_implDelegate; }),
        m_implDelegate( param1 )
    {
    }

    template<class Param1Type, class Param2Type>
    AbstractSocketImplementationEmbeddingDelegate(const Param1Type& param1, const Param2Type& param2)
    :
        base_type([this](){ return &m_implDelegate; }),
        m_implDelegate(param1, param2)
    {
    }

    template<class Param1Type, class Param2Type, class Param3Type>
    AbstractSocketImplementationEmbeddingDelegate(const Param1Type& param1, const Param2Type& param2, const Param3Type& param3)
    :
        base_type([this](){ return &m_implDelegate; }),
        m_implDelegate(param1, param2, param3)
    {
    }

    template<class Param1Type, class Param2Type, class Param3Type>
    AbstractSocketImplementationEmbeddingDelegate(AbstractCommunicatingSocket* abstractSocketPtr, const Param1Type& param1, const Param2Type& param2, const Param3Type& param3)
    :
        base_type([this](){ return &m_implDelegate; }),
        m_implDelegate(abstractSocketPtr, param1, param2, param3)
    {
    }

    template<class Param1Type, class Param2Type, class Param3Type, class Param4Type>
    AbstractSocketImplementationEmbeddingDelegate(
        AbstractCommunicatingSocket* abstractSocketPtr,
        const Param1Type& param1,
        const Param2Type& param2,
        const Param3Type& param3,
        const Param4Type& param4)
    :
        base_type([this](){ return &m_implDelegate; }),
        m_implDelegate(abstractSocketPtr, param1, param2, param3, param4)
    {
    }

    AbstractSocketImplementorType* implementationDelegate() { return &m_implDelegate; }
    const AbstractSocketImplementorType* implementationDelegate() const { return &m_implDelegate; }

protected:
    AbstractSocketImplementorType m_implDelegate;
};



//!Implements pure virtual methods of \a AbstractCommunicatingSocket by delegating them to \a CommunicatingSocket
template<typename AbstractBaseType, typename AbstractCommunicatingSocketImplementorType>
class AbstractCommunicatingSocketImplementationDelegate
:
    public AbstractSocketImplementationEmbeddingDelegate<AbstractBaseType, AbstractCommunicatingSocketImplementorType>
{
    typedef AbstractSocketImplementationEmbeddingDelegate<AbstractBaseType, AbstractCommunicatingSocketImplementorType> base_type;

public:
    //TODO #ak replace multiple constructors with variadic template after move to msvc2013
    template<class Param1Type>
    AbstractCommunicatingSocketImplementationDelegate(const Param1Type& param1)
    :
        base_type( this, param1 )
    {
    }

    template<class Param1Type, class Param2Type>
    AbstractCommunicatingSocketImplementationDelegate(const Param1Type& param1, const Param2Type& param2)
    :
        base_type( this, param1, param2 )
    {
    }

    template<class Param1Type, class Param2Type, class Param3Type>
    AbstractCommunicatingSocketImplementationDelegate(const Param1Type& param1, const Param2Type& param2, const Param3Type& param3)
    :
        base_type( this, param1, param2, param3 )
    {
    }

    template<class Param1Type, class Param2Type, class Param3Type, class Param4Type>
    AbstractCommunicatingSocketImplementationDelegate(const Param1Type& param1, const Param2Type& param2, const Param3Type& param3, const Param4Type& param4)
    :
        base_type( this, param1, param2, param3, param4 )
    {
    }

    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractCommunicatingSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of AbstractCommunicatingSocket::connect
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis ) override
    {
        return this->m_implDelegate.connect( remoteAddress, timeoutMillis );
    }
    //!Implementation of AbstractCommunicatingSocket::recv
    virtual int recv( void* buffer, unsigned int bufferLen, int flags ) override { return this->m_implDelegate.recv( buffer, bufferLen, flags ); }
    //!Implementation of AbstractCommunicatingSocket::send
    virtual int send( const void* buffer, unsigned int bufferLen ) override { return this->m_implDelegate.send( buffer, bufferLen ); }
    //!Implementation of AbstractCommunicatingSocket::getForeignAddress
    virtual SocketAddress getForeignAddress() const override { return this->m_implDelegate.getForeignAddress(); }
    //!Implementation of AbstractCommunicatingSocket::isConnected
    virtual bool isConnected() const override { return this->m_implDelegate.isConnected(); }
    //!Implementation of AbstractCommunicatingSocket::connectAsync
    virtual void connectAsync(
        const SocketAddress& addr,
        std::function<void( SystemError::ErrorCode )> handler ) override
    {
        return this->m_implDelegate.connectAsync( addr, std::move(handler) );
    }
    //!Implementation of AbstractCommunicatingSocket::readSomeAsync
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override
    {
        return this->m_implDelegate.readSomeAsync( buf, std::move( handler ) );
    }
    //!Implementation of AbstractCommunicatingSocket::sendAsync
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override
    {
        return this->m_implDelegate.sendAsync( buf, std::move( handler ) );
    }
    //!Implementation of AbstractCommunicatingSocket::registerTimer
    virtual void registerTimer(
        unsigned int timeoutMs,
        std::function<void()> handler ) override
    {
        return this->m_implDelegate.registerTimer( timeoutMs, std::move( handler ) );
    }
    virtual void cancelIOAsync(
        aio::EventType eventType,
        std::function<void()> cancellationDoneHandler) override
    {
        return this->m_implDelegate.cancelIOAsync(eventType, std::move(cancellationDoneHandler));
    }
    virtual void cancelIOSync(aio::EventType eventType) override
    {
        return this->m_implDelegate.cancelIOSync(eventType);
    }
};

#endif  //SOCKET_IMPL_HELPER_H
