#pragma once

#include <nx/network/abstract_socket.h>

#include "../aio/basic_pollable.h"

namespace nx {
namespace network {

/**
 * Base class for socket, reading/writing data from/to any source (e.g., file).
 */
class NX_NETWORK_API DummySocket:
    public AbstractStreamSocket
{
    using base_type = AbstractStreamSocket;

public:
    DummySocket();

    virtual bool bind( const SocketAddress& localAddress ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool setReuseAddrFlag( bool reuseAddr ) override;
    virtual bool getReuseAddrFlag( bool* val ) const override;
    virtual bool setReusePortFlag(bool value) override;
    virtual bool getReusePortFlag(bool* value) const override;
    virtual bool setNonBlockingMode( bool val ) override;
    virtual bool getNonBlockingMode( bool* val ) const override;
    virtual bool getMtu( unsigned int* mtuValue ) const  override;
    virtual bool setSendBufferSize( unsigned int buffSize ) override;
    virtual bool getSendBufferSize( unsigned int* buffSize ) const override;
    virtual bool setRecvBufferSize( unsigned int buffSize ) override;
    virtual bool getRecvBufferSize( unsigned int* buffSize ) const override;
    virtual bool setRecvTimeout( unsigned int millis ) override;
    virtual bool getRecvTimeout( unsigned int* millis ) const override;
    virtual bool setSendTimeout( unsigned int ms ) override;
    virtual bool getSendTimeout( unsigned int* millis ) const override;
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const  override;
    virtual bool setIpv6Only( bool val ) override;
    virtual bool getProtocol(int* protocol) const override;

    virtual SOCKET_HANDLE handle() const override;
    virtual Pollable* pollable() override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override;

    virtual SocketAddress getForeignAddress() const override;

    virtual bool setNoDelay( bool value ) override;
    virtual bool getNoDelay( bool* value ) const override;
    virtual bool toggleStatisticsCollection( bool val ) override;
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;
    virtual bool setKeepAlive( std::optional< KeepAliveOptions > info ) override;
    virtual bool getKeepAlive( std::optional< KeepAliveOptions >* result ) const override;

    virtual void post( nx::utils::MoveOnlyFunc<void()> handler ) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler ) override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler ) override;

    virtual void readSomeAsync(
        nx::Buffer* const buf,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode, size_t )> handler ) override;

    virtual void sendAsync(
        const nx::Buffer& buf,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode, size_t )> handler ) override;

    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler ) override;

    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

protected:
    virtual void cancelIoInAioThread(aio::EventType eventType) override;

private:
    SocketAddress m_localAddress;
    SocketAddress m_remotePeerAddress;
    aio::BasicPollable m_basicPollable;
};

} // namespace network
} // namespace nx
