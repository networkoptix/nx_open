#pragma once

#include <memory>

#include "abstract_socket.h"

namespace nx {
namespace network {

/**
 * Base for some class that wants to extend socket functionality a bit 
 * and delegate rest of API calls to existing implementation.
 */
template<typename SocketInterfaceToImplement>
class SocketDelegate:
    public SocketInterfaceToImplement
{
    static_assert(
        std::is_base_of<AbstractSocket, SocketInterfaceToImplement>::value,
        "You MUST use class derived of AbstractSocket as a template argument");

public:
    SocketDelegate(std::unique_ptr<SocketInterfaceToImplement> target):
        m_target(std::move(target))
    {
    }

    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override
    {
        return m_target->getLastError(errorCode);
    }

    virtual AbstractSocket::SOCKET_HANDLE handle() const override
    {
        return m_target->handle();
    }

    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        return m_target->getRecvTimeout(millis);
    }

    virtual bool getSendTimeout(unsigned int* millis) const override
    {
        return m_target->getSendTimeout(millis);
    }

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override
    {
        return m_target->getAioThread();
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        return m_target->bindToAioThread(aioThread);
    }

    virtual bool isInSelfAioThread() const override
    {
        return m_target->isInSelfAioThread();
    }

    virtual bool bind(const SocketAddress& localAddress) override
    {
        return m_target->bind(localAddress);
    }

    virtual SocketAddress getLocalAddress() const override
    {
        return m_target->getLocalAddress();
    }

    virtual bool close() override
    {
        return m_target->close();
    }

    virtual bool shutdown() override
    {
        return m_target->shutdown();
    }

    virtual bool isClosed() const override
    {
        return m_target->isClosed();
    }

    virtual bool setReuseAddrFlag(bool reuseAddr) override
    {
        return m_target->setReuseAddrFlag(reuseAddr);
    }

    virtual bool getReuseAddrFlag(bool* val) const override
    {
        return m_target->getReuseAddrFlag(val);
    }

    virtual bool setNonBlockingMode(bool val) override
    {
        return m_target->setNonBlockingMode(val);
    }

    virtual bool getNonBlockingMode(bool* val) const override
    {
        return m_target->getNonBlockingMode(val);
    }

    virtual bool getMtu(unsigned int* mtuValue) const override
    {
        return m_target->getMtu(mtuValue);
    }

    virtual bool setSendBufferSize(unsigned int buffSize) override
    {
        return m_target->setSendBufferSize(buffSize);
    }

    virtual bool getSendBufferSize(unsigned int* buffSize) const override
    {
        return m_target->getSendBufferSize(buffSize);
    }

    virtual bool setRecvBufferSize(unsigned int buffSize) override
    {
        return m_target->setRecvBufferSize(buffSize);
    }

    virtual bool getRecvBufferSize(unsigned int* buffSize) const override
    {
        return m_target->getRecvBufferSize(buffSize);
    }

    virtual bool setRecvTimeout(unsigned int ms) override
    {
        return m_target->setRecvTimeout(ms);
    }

    virtual bool setSendTimeout(unsigned int ms) override
    {
        return m_target->setSendTimeout(ms);
    }

    virtual Pollable* pollable() override
    {
        return m_target->pollable();
    }

    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        return m_target->post(std::move(handler));
    }

    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        return m_target->dispatch(std::move(handler));
    }

protected:
    std::unique_ptr<SocketInterfaceToImplement> m_target;
};

template<typename SocketInterfaceToImplement>
class CommunicatingSocketDelegate:
    public SocketDelegate<SocketInterfaceToImplement>
{
    static_assert(
        std::is_base_of<AbstractCommunicatingSocket, SocketInterfaceToImplement>::value,
        "You MUST use class derived of AbstractCommunicatingSocket as a template argument");

    using base_type = SocketDelegate<SocketInterfaceToImplement>;

public:
    CommunicatingSocketDelegate(std::unique_ptr<SocketInterfaceToImplement> target):
        base_type(std::move(target))
    {
    }

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = AbstractCommunicatingSocket::kDefaultTimeoutMillis) override
    {
        return this->m_target->connect(remoteSocketAddress, timeoutMillis);
    }

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override
    {
        return this->m_target->recv(buffer, bufferLen, flags);
    }

    virtual int send(const void* buffer, unsigned int bufferLen) override
    {
        return this->m_target->send(buffer, bufferLen);
    }

    virtual SocketAddress getForeignAddress() const override
    {
        return this->m_target->getForeignAddress();
    }

    virtual bool isConnected() const override
    {
        return this->m_target->isConnected();
    }

    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        return this->m_target->connectAsync(address, std::move(handler));
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        return this->m_target->readSomeAsync(buffer, std::move(handler));
    }

    void readAsyncAtLeast(
        nx::Buffer* const buffer, size_t minimalSize,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        return this->m_target->readAsyncAtLeast(buffer, minimalSize, std::move(handler));
    }

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        return this->m_target->sendAsync(buffer, std::move(handler));
    }

    virtual void registerTimer(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> handler) override
    {
        return this->m_target->registerTimer(timeout, std::move(handler));
    }

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override
    {
        return this->m_target->cancelIOAsync(eventType, std::move(handler));
    }

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override
    {
        return this->m_target->cancelIOSync(eventType);
    }
};

class StreamSocketDelegate:
    public CommunicatingSocketDelegate<AbstractStreamSocket>
{
    using base_type = CommunicatingSocketDelegate<AbstractStreamSocket>;

public:
    StreamSocketDelegate(std::unique_ptr<AbstractStreamSocket> target):
        base_type(std::move(target))
    {
    }

    virtual bool reopen() override
    {
        return this->m_target->reopen();
    }

    virtual bool setNoDelay(bool value) override
    {
        return this->m_target->setNoDelay(value);
    }

    virtual bool getNoDelay(bool* value) const override
    {
        return this->m_target->getNoDelay(value);
    }

    virtual bool toggleStatisticsCollection(bool val) override
    {
        return this->m_target->toggleStatisticsCollection(val);
    }

    virtual bool getConnectionStatistics(StreamSocketInfo* info) override
    {
        return this->m_target->getConnectionStatistics(info);
    }

    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > info) override
    {
        return this->m_target->setKeepAlive(info);
    }

    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* result) const override
    {
        return this->m_target->getKeepAlive(result);
    }
};

class StreamServerSocketDelegate:
    public SocketDelegate<AbstractStreamServerSocket>
{
    using base_type = SocketDelegate<AbstractStreamServerSocket>;

public:
    StreamServerSocketDelegate(std::unique_ptr<AbstractStreamServerSocket> target):
        base_type(std::move(target))
    {
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
    {
        this->m_target->pleaseStop(std::move(handler));
    }

    virtual void pleaseStopSync(bool assertIfCalledUnderLock = true)
    {
        this->m_target->pleaseStopSync(assertIfCalledUnderLock);
    }

    virtual bool listen(int backlog = kDefaultBacklogSize) override
    {
        return this->m_target->listen(backlog);
    }

    virtual AbstractStreamSocket* accept()
    {
        return this->m_target->accept();
    }

    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler)
    {
        return this->m_target->acceptAsync(std::move(handler));
    }

    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
    {
        return this->m_target->cancelIOAsync(std::move(handler));
    }

    virtual void cancelIOSync()
    {
        return this->m_target->cancelIOSync();
    }
};

} // namespace network
} // namespace nx
