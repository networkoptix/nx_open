#pragma once

#include <nx/network/aio/timer.h>

#include "../custom_handshake_connection_acceptor.h"
#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

namespace detail {

class NX_NETWORK_API AbstractAcceptedSslStreamSocketWrapper:
    public CustomStreamSocketDelegate<AbstractEncryptedStreamSocket, AbstractStreamSocket>
{
    using base_type =
        CustomStreamSocketDelegate<AbstractEncryptedStreamSocket, AbstractStreamSocket>;

public:
    template<typename ...Args>
    AbstractAcceptedSslStreamSocketWrapper(Args... args):
        base_type(std::move(args)...)
    {
    }
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

enum class EncryptionUse
{
    always,
    autoDetectByReceivedData,
    never,
};

/**
 * NOTE: Provides connections that have already completed SSL handshake.
 *   So, Accepting TCP connections and performing SSL handshake is done
 *   right after StreamServerSocket::listen call.
 */
class NX_NETWORK_API StreamServerSocket:
    public StreamServerSocketDelegate
{
    using base_type = StreamServerSocketDelegate;

public:
    StreamServerSocket(
        std::unique_ptr<AbstractStreamServerSocket> delegate,
        EncryptionUse encryptionUse);
    ~StreamServerSocket();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync() override;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual bool setNonBlockingMode(bool value) override;
    virtual bool getNonBlockingMode(bool* value) const override;

    virtual bool listen(int backlog = kDefaultBacklogSize) override;
    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual std::unique_ptr<AbstractStreamSocket> accept() override;

protected:
    virtual void cancelIoInAioThread() override;

private:
    using Acceptor = CustomHandshakeConnectionAcceptor<
        AbstractStreamServerSocket,
        detail::AbstractAcceptedSslStreamSocketWrapper>;

    Acceptor m_acceptor;
    aio::Timer m_timer;
    EncryptionUse m_encryptionUse;
    AcceptCompletionHandler m_userHandler;
    bool m_nonBlockingModeEnabled = false;

    std::unique_ptr<detail::AbstractAcceptedSslStreamSocketWrapper> createSocketWrapper(
        std::unique_ptr<AbstractStreamSocket> delegate);

    std::unique_ptr<AbstractStreamSocket> acceptNonBlocking();
    std::unique_ptr<AbstractStreamSocket> acceptBlocking();

    void acceptAsyncInternal(AcceptCompletionHandler handler);

    void startTimer(std::chrono::milliseconds timeout);

    void onAccepted(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection);

    void stopWhileInAioThread();
};

} // namespace ssl
} // namespace network
} // namespace nx
