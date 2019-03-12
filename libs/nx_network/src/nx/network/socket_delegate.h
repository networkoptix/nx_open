#pragma once

#include <memory>

#include "abstract_socket.h"

namespace nx {
namespace network {

/**
 * Base for some class that wants to extend socket functionality a bit
 * and delegate rest of API calls to existing implementation.
 */
template<typename SocketInterfaceToImplement, typename TargetType>
// requires std::is_base_of<AbstractSocket, SocketInterfaceToImplement>::value
class SocketDelegate:
    public SocketInterfaceToImplement
{
    static_assert(
        std::is_base_of<AbstractSocket, SocketInterfaceToImplement>::value,
        "You MUST use class derived of AbstractSocket as a template argument");

public:
    SocketDelegate(TargetType* target):
        m_target(target)
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

    virtual bool setReusePortFlag(bool value) override
    {
        return m_target->setReusePortFlag(value);
    }

    virtual bool getReusePortFlag(bool* value) const override
    {
        return m_target->getReusePortFlag(value);
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

    virtual bool setIpv6Only(bool val) override
    {
        return m_target->setIpv6Only(val);
    }

    virtual bool getProtocol(int* protocol) const override
    {
        return m_target->getProtocol(protocol);
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
    TargetType* m_target;
};

template<typename SocketInterfaceToImplement, typename TargetType>
class CommunicatingSocketDelegate:
    public SocketDelegate<SocketInterfaceToImplement, TargetType>
{
    static_assert(
        std::is_base_of<AbstractCommunicatingSocket, SocketInterfaceToImplement>::value,
        "You MUST use class derived of AbstractCommunicatingSocket as a template argument");

    using base_type = SocketDelegate<SocketInterfaceToImplement, TargetType>;

public:
    CommunicatingSocketDelegate(TargetType* target):
        base_type(target)
    {
    }

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override
    {
        return this->m_target->connect(remoteSocketAddress, timeout);
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

    virtual QString getForeignHostName() const override
    {
        return this->m_target->getForeignHostName();
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
        IoCompletionHandler handler) override
    {
        return this->m_target->readSomeAsync(buffer, std::move(handler));
    }

    /*
     * Warning! Buffer should live at least till asynchronous send occurres, so
     * do not use buffers with local scope here.
     */
    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) override
    {
        return this->m_target->sendAsync(buffer, std::move(handler));
    }

    virtual void registerTimer(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> handler) override
    {
        return this->m_target->registerTimer(timeout, std::move(handler));
    }

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override
    {
        return this->m_target->cancelIOSync(eventType);
    }
};

/**
 * Does not takes ownership.
 */
template<typename SocketInterfaceToImplement, typename TargetType>
class CustomStreamSocketDelegate:
    public CommunicatingSocketDelegate<SocketInterfaceToImplement, TargetType>
{
    using base_type = CommunicatingSocketDelegate<SocketInterfaceToImplement, TargetType>;

public:
    CustomStreamSocketDelegate(TargetType* target):
        base_type(target)
    {
    }

    virtual bool setNoDelay(bool value) override
    {
        return this->m_target->setNoDelay(value);
    }

    virtual bool getNoDelay(bool* value) const override
    {
        return this->m_target->getNoDelay(value);
    }

    virtual bool toggleStatisticsCollection(bool value) override
    {
        return this->m_target->toggleStatisticsCollection(value);
    }

    virtual bool getConnectionStatistics(StreamSocketInfo* info) override
    {
        return this->m_target->getConnectionStatistics(info);
    }

    virtual bool setKeepAlive(std::optional< KeepAliveOptions > info) override
    {
        return this->m_target->setKeepAlive(info);
    }

    virtual bool getKeepAlive(std::optional< KeepAliveOptions >* result) const override
    {
        return this->m_target->getKeepAlive(result);
    }
};

class NX_NETWORK_API StreamSocketDelegate:
    public CustomStreamSocketDelegate<AbstractStreamSocket, AbstractStreamSocket>
{
    using base_type =
        CustomStreamSocketDelegate<AbstractStreamSocket, AbstractStreamSocket>;

public:
    StreamSocketDelegate(AbstractStreamSocket* target);
};

class NX_NETWORK_API StreamServerSocketDelegate:
    public SocketDelegate<AbstractStreamServerSocket, AbstractStreamServerSocket>
{
    using base_type = SocketDelegate<AbstractStreamServerSocket, AbstractStreamServerSocket>;

public:
    StreamServerSocketDelegate(AbstractStreamServerSocket* target);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync() override;
    virtual bool listen(int backlog = kDefaultBacklogSize) override;
    virtual std::unique_ptr<AbstractStreamSocket> accept() override;
    virtual void acceptAsync(AcceptCompletionHandler handler) override;

protected:
    virtual void cancelIoInAioThread() override;
};

} // namespace network
} // namespace nx
