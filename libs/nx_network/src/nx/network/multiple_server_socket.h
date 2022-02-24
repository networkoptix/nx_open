// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>
#include <queue>

#include <nx/utils/std/cpp14.h>

#include "aggregate_acceptor.h"
#include "aio/timer.h"
#include "system_socket.h"

namespace nx::network {

/**
 * Accepts connections from multiple given AbstractStreamServerSocket instances.
 * Add new socket with a MultipleServerSocket::addSocket call.
 */
class NX_NETWORK_API MultipleServerSocket:
    public AbstractStreamServerSocket
{
public:
    MultipleServerSocket();
    ~MultipleServerSocket();

    //TODO #muskov does not compile on msvc2012
    //template<typename S1, typename S2, typename = typename std::enable_if<
    //    !(std::is_base_of<S1, S2>::value && std::is_base_of<S2, S1>::value)>::type>
    template<typename S1, typename S2>
    std::unique_ptr<AbstractStreamServerSocket> make();

    //---------------------------------------------------------------------------------------------
    // Implementation of AbstractSocket::*
    virtual Pollable* pollable() override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual bool bind(const SocketAddress& localAddress) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool shutdown() override;
    virtual bool isClosed() const override;
    virtual bool setReuseAddrFlag(bool reuseAddr) override;
    virtual bool getReuseAddrFlag(bool* val) const override;
    virtual bool setReusePortFlag(bool value) override;
    virtual bool getReusePortFlag(bool* value) const override;
    virtual bool setNonBlockingMode(bool val) override;
    virtual bool getNonBlockingMode(bool* val) const override;
    virtual bool getMtu(unsigned int* mtuValue) const override;
    virtual bool setSendBufferSize(unsigned int buffSize) override;
    virtual bool getSendBufferSize(unsigned int* buffSize) const override;
    virtual bool setRecvBufferSize(unsigned int buffSize) override;
    virtual bool getRecvBufferSize(unsigned int* buffSize) const override;
    virtual bool setRecvTimeout(unsigned int millis) override;
    virtual bool getRecvTimeout(unsigned int* millis) const override;
    virtual bool setSendTimeout(unsigned int ms) override;
    virtual bool getSendTimeout(unsigned int* millis) const override;
    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override;
    virtual bool setIpv6Only(bool val) override;
    virtual bool getProtocol(int* protocol) const override;

    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual bool isInSelfAioThread() const override;

    //---------------------------------------------------------------------------------------------
    // Implementation of QnStoppable::*
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync() override;

    //---------------------------------------------------------------------------------------------
    // Implementation of AbstractStreamServerSocket::*
    virtual bool listen(int queueLen) override;
    virtual std::unique_ptr<AbstractStreamSocket> accept() override;
    virtual void acceptAsync(AcceptCompletionHandler handler) override;

    /**
     * These methods can be called concurrently with MultipleServerSocket::accept.
     * NOTE: Blocks until completion.
     */
    bool addSocket(std::unique_ptr<AbstractStreamServerSocket> socket);
    void removeSocket(size_t pos);
    size_t count() const;

protected:
    bool m_nonBlockingMode = false;
    unsigned int m_recvTmeout = 0;
    mutable SystemError::ErrorCode m_lastError;
    bool* m_terminated = nullptr;
    aio::Timer m_timer;
    std::vector<AbstractStreamServerSocket*> m_serverSockets;
    AcceptCompletionHandler m_acceptHandler;
    AggregateAcceptor m_aggregateAcceptor;

    virtual void cancelIoInAioThread() override;

private:
    void stopWhileInAioThread();
};

template<typename S1, typename S2/*, typename*/>
std::unique_ptr<AbstractStreamServerSocket> MultipleServerSocket::make()
{
    std::unique_ptr<MultipleServerSocket> self(new MultipleServerSocket);

    if (!self->addSocket(std::unique_ptr<S1>(new S1)))
        return nullptr;

    if (!self->addSocket(std::unique_ptr<S2>(new S2)))
        return nullptr;

    return std::move(self);
}

} // namespace nx::network
