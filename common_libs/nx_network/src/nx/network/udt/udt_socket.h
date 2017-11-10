#pragma once

#include <memory>

#include <boost/optional.hpp>

#include "../abstract_socket.h"
#include "../aio/event_type.h"
#include "../socket_common.h"
#include "../system_socket.h"
#include "../aio/event_type.h"

namespace nx {
namespace network {

namespace aio {

template<class SocketType> class AsyncSocketImplHelper;
template<class SocketType> class AsyncServerSocketHelper;

} // namespace aio

  // I put the implementator inside of detail namespace to avoid namespace pollution.
  // The reason is that I see many of the class prefer using Implementator , maybe
  // we want binary compatible of our source code. Anyway, this is not a bad thing
  // but some sacrifice on inline function.
namespace detail {

class UdtSocketImpl;
enum class SocketState
{
    closed,
    open,
    connected
};

} // namespace detail

template<class InterfaceToImplement>
class UdtSocket:
    public Pollable,
    public InterfaceToImplement
{
public:
    UdtSocket(int ipVersion);
    virtual ~UdtSocket();

    /**
    * Binds UDT socket to an existing UDP socket.
    * @note This method can be called just after UdtSocket creation.
    * @note if method have failed UdtSocket instance MUST be destroyed!
    */
    bool bindToUdpSocket(UDPSocket&& udpSocket);

    // AbstractSocket --------------- interface
    virtual bool bind(const SocketAddress& localAddress) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool shutdown() override;
    virtual bool isClosed() const override;
    virtual bool setReuseAddrFlag(bool reuseAddr) override;
    virtual bool getReuseAddrFlag(bool* val) const override;
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

    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual bool isInSelfAioThread() const override;
    virtual Pollable* pollable() override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;

protected:
    friend class UdtPollSet;

    detail::SocketState m_state;
    detail::UdtSocketImpl* m_impl;
    const int m_ipVersion;

    UdtSocket(
        int ipVersion,
        detail::UdtSocketImpl* impl,
        detail::SocketState state);

    bool open();

    UdtSocket(const UdtSocket&);
    UdtSocket& operator=(const UdtSocket&);
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
        detail::UdtSocketImpl* impl,
        detail::SocketState state);
    // We must declare this trivial constructor even it is trivial.
    // Since this will make std::unique_ptr call correct destructor for our
    // partial, forward declaration of class UdtSocketImp;
    virtual ~UdtStreamSocket();

    bool setRendezvous(bool val);

    // AbstractCommunicatingSocket ------- interface
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = kDefaultTimeoutMillis) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;
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
        IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buf,
        IoCompletionHandler handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMillis,
        nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    bool connectToIp(const SocketAddress& remoteAddress, unsigned int timeoutMillis);
    /**
    * @return false if failed to read socket options.
    */
    bool checkIfRecvModeSwitchIsRequired(int flags, boost::optional<bool>* requiredRecvMode);
    bool setRecvMode(bool isRecvSync);
    int handleRecvResult(int recvResult);

    std::unique_ptr<aio::AsyncSocketImplHelper<UdtStreamSocket>> m_aioHelper;
    bool m_noDelay = false;
    bool m_isInternetConnection = false;

private:
    Q_DISABLE_COPY(UdtStreamSocket)
};

class NX_NETWORK_API UdtStreamServerSocket:
    public UdtSocket<AbstractStreamServerSocket>
{
    using base_type = UdtSocket<AbstractStreamServerSocket>;

public:
    explicit UdtStreamServerSocket(int ipVersion = AF_INET);
    virtual ~UdtStreamServerSocket();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool assertIfCalledUnderLock = true) override;

    // AbstractStreamServerSocket -------------- interface
    virtual bool listen(int queueLen = 128) override;
    virtual AbstractStreamSocket* accept() override;
    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync() override;

    /** This method is for use by \a AsyncServerSocketHelper only. It just calls system call \a accept */
    AbstractStreamSocket* systemAccept();

private:
    std::unique_ptr<aio::AsyncServerSocketHelper<UdtStreamServerSocket>> m_aioHelper;

    void stopWhileInAioThread();

    Q_DISABLE_COPY(UdtStreamServerSocket)
};

class NX_NETWORK_API UdtStatistics
{
public:
#ifdef __arm__
    std::atomic<uint32_t> internetBytesTransfered{ 0 };
#else
    std::atomic<uint64_t> internetBytesTransfered{ 0 };
#endif

    static UdtStatistics global;
};

} // namespace network
} // namespace nx
