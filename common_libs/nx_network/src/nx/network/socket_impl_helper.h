#pragma once

#include "abstract_socket.h"

namespace nx {
namespace network {

/**
 * @param AbstractSocketProviderType functor with operator() that
 * returns type that implements methods of AbstractSocket class.
 */
template<class AbstractBaseType, class AbstractSocketProviderType>
class AbstractSocketImplementationDelegate:
    public AbstractBaseType
{
public:
    AbstractSocketImplementationDelegate(AbstractSocketProviderType abstractSocketProvider):
        m_abstractSocketProvider(abstractSocketProvider)
    {
    }

    virtual bool bind(const SocketAddress& localAddress) override { return m_abstractSocketProvider()->bind(localAddress); }
    virtual SocketAddress getLocalAddress() const override { return m_abstractSocketProvider()->getLocalAddress(); }
    virtual bool close() override { return m_abstractSocketProvider()->close(); }
    virtual bool shutdown() override { return m_abstractSocketProvider()->shutdown(); }
    virtual bool isClosed() const override { return m_abstractSocketProvider()->isClosed(); }
    virtual bool setReuseAddrFlag(bool reuseAddr) override { return m_abstractSocketProvider()->setReuseAddrFlag(reuseAddr); }
    virtual bool getReuseAddrFlag(bool* val) const override { return m_abstractSocketProvider()->getReuseAddrFlag(val); }
    virtual bool setNonBlockingMode(bool val) override { return m_abstractSocketProvider()->setNonBlockingMode(val); }
    virtual bool getNonBlockingMode(bool* val) const override { return m_abstractSocketProvider()->getNonBlockingMode(val); }
    virtual bool getMtu(unsigned int* mtuValue) const override { return m_abstractSocketProvider()->getMtu(mtuValue); }
    virtual bool setSendBufferSize(unsigned int buffSize) override { return m_abstractSocketProvider()->setSendBufferSize(buffSize); }
    virtual bool getSendBufferSize(unsigned int* buffSize) const override { return m_abstractSocketProvider()->getSendBufferSize(buffSize); }
    virtual bool setRecvBufferSize(unsigned int buffSize) override { return m_abstractSocketProvider()->setRecvBufferSize(buffSize); }
    virtual bool getRecvBufferSize(unsigned int* buffSize) const override { return m_abstractSocketProvider()->getRecvBufferSize(buffSize); }
    virtual bool setRecvTimeout(unsigned int ms) override { return m_abstractSocketProvider()->setRecvTimeout(ms); }
    virtual bool getRecvTimeout(unsigned int* millis) const override { return m_abstractSocketProvider()->getRecvTimeout(millis); }
    virtual bool setSendTimeout(unsigned int ms) override { return m_abstractSocketProvider()->setSendTimeout(ms); }
    virtual bool getSendTimeout(unsigned int* millis) const override { return m_abstractSocketProvider()->getSendTimeout(millis); }
    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override { return m_abstractSocketProvider()->getLastError(errorCode); }
    virtual AbstractSocket::SOCKET_HANDLE handle() const override { return m_abstractSocketProvider()->handle(); }
    virtual Pollable* pollable() override { return m_abstractSocketProvider()->pollable(); }
    virtual void post( nx::utils::MoveOnlyFunc<void()> handler ) override { m_abstractSocketProvider()->post( std::move(handler) ); }
    virtual void dispatch( nx::utils::MoveOnlyFunc<void()> handler ) override { m_abstractSocketProvider()->dispatch( std::move(handler) ); }
    virtual nx::network::aio::AbstractAioThread* getAioThread() const override { return m_abstractSocketProvider()->getAioThread(); }
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override { m_abstractSocketProvider()->bindToAioThread( aioThread ); }
    virtual bool isInSelfAioThread() const override { return m_abstractSocketProvider()->isInSelfAioThread(); }

private:
    AbstractSocketProviderType m_abstractSocketProvider;
};

} // namespace network
} // namespace nx
