// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include "../abstract_socket.h"
#include "../aio/event_type.h"
#include "../socket_common.h"
#include "../system_socket.h"

namespace nx {
namespace network {

namespace aio {

template<class SocketType> class AsyncSocketImplHelper;
template<class SocketType> class AsyncServerSocketHelper;

} // namespace aio

namespace detail {

enum class SocketState
{
    closed,
    open,
    connected
};

} // namespace detail

class UdtSocketImpl;

template<class InterfaceToImplement>
class NX_NETWORK_API UdtSocket:
    public Pollable,
    public InterfaceToImplement
{
public:
    UdtSocket(aio::AIOService* aioService, int ipVersion);
    virtual ~UdtSocket();

    /**
     * Binds UDT socket to an existing UDP socket.
     * Takes ownership of the handler of udpSocket.
     * If successful, then udpSocket has no system socket handler and is unusable.
     * NOTE: This method can be called just after UdtSocket creation.
     * NOTE: if method have failed UdtSocket instance MUST be destroyed!
     */
    bool bindToUdpSocket(AbstractDatagramSocket* udpSocket);

    // AbstractSocket.
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
    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual bool isInSelfAioThread() const override;
    virtual Pollable* pollable() override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;

protected:
    friend class UdtPollSet;

    aio::AIOService* m_aioService = nullptr;
    detail::SocketState m_state;
    UdtSocketImpl* m_impl = nullptr;
    const int m_ipVersion;

    UdtSocket(
        aio::AIOService* aioService,
        int ipVersion,
        std::unique_ptr<UdtSocketImpl> impl,
        detail::SocketState state);

    bool open();

    UdtSocket(const UdtSocket&) = delete;
    UdtSocket& operator=(const UdtSocket&) = delete;
};

// BTW: Why some getter function has const qualifier, and others don't have this in AbstractStreamSocket ??

class NX_NETWORK_API UdtStreamSocket:
    public UdtSocket<AbstractStreamSocket>
{
    using base_type = UdtSocket<AbstractStreamSocket>;

public:
    explicit UdtStreamSocket(int ipVersion = AF_INET);
    UdtStreamSocket(
        int ipVersion,
        std::unique_ptr<UdtSocketImpl> impl,
        detail::SocketState state);
    virtual ~UdtStreamSocket();

    UdtStreamSocket(const UdtStreamSocket&) = delete;
    UdtStreamSocket& operator=(const UdtStreamSocket&) = delete;

    bool setRendezvous(bool val);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    // AbstractCommunicatingSocket.
    virtual bool connect(
        const SocketAddress& remoteAddress,
        std::chrono::milliseconds timeout) override;

    virtual int recv(void* buffer, std::size_t bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, std::size_t bufferLen) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;

    // AbstractStreamSocket.
    virtual bool setNoDelay(bool value) override;
    virtual bool getNoDelay(bool* /*value*/) const override;
    virtual bool toggleStatisticsCollection(bool val) override;
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override;
    virtual bool setKeepAlive(std::optional< KeepAliveOptions > info) override;
    virtual bool getKeepAlive(std::optional< KeepAliveOptions >* result) const override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer* buf,
        IoCompletionHandler handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMillis,
        nx::utils::MoveOnlyFunc<void()> handler) override;

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

private:
    bool connectToIp(
        const SocketAddress& remoteAddress,
        std::chrono::milliseconds timeout);
    /**
     * @return false if failed to read socket options.
     */
    bool checkIfRecvModeSwitchIsRequired(int flags, std::optional<bool>* requiredRecvMode);
    bool setRecvMode(bool isRecvSync);
    int handleRecvResult(int recvResult);

    std::unique_ptr<aio::AsyncSocketImplHelper<UdtStreamSocket>> m_aioHelper;
    bool m_noDelay = false;
    bool m_isInternetConnection = false;
};

class NX_NETWORK_API UdtStreamServerSocket:
    public UdtSocket<AbstractStreamServerSocket>
{
    using base_type = UdtSocket<AbstractStreamServerSocket>;

public:
    explicit UdtStreamServerSocket(int ipVersion = AF_INET);
    virtual ~UdtStreamServerSocket();

    UdtStreamServerSocket(const UdtStreamServerSocket&) = delete;
    UdtStreamServerSocket& operator=(const UdtStreamServerSocket&) = delete;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync() override;

    // AbstractStreamServerSocket.
    virtual bool listen(int queueLen = 128) override;
    virtual std::unique_ptr<AbstractStreamSocket> accept() override;
    virtual void acceptAsync(AcceptCompletionHandler handler) override;

    /**
     * For use by AsyncServerSocketHelper only.
     * It just calls system call accept.
     */
    std::unique_ptr<AbstractStreamSocket> systemAccept();

protected:
    virtual void cancelIoInAioThread() override;

private:
    std::unique_ptr<aio::AsyncServerSocketHelper<UdtStreamServerSocket>> m_aioHelper;

    void stopWhileInAioThread();
};

class NX_NETWORK_API UdtStatistics
{
public:
    #if defined(__arm__) //< Some 32-bit ARM devices lack the kernel support for the atomic int64.
        std::atomic<uint32_t> internetBytesTransferred{0};
    #else
        std::atomic<uint64_t> internetBytesTransferred{0};
    #endif

    static UdtStatistics global;
};

} // namespace network
} // namespace nx
