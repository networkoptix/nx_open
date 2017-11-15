#pragma once

#include <atomic>
#include <list>
#include <memory>

#include <nx/network/aio/timer.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/thread/mutex.h>

#include "../abstract_outgoing_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace udp {

class Timeouts
{
public:
    std::chrono::seconds keepAlivePeriod;
    /** Number of missing keep-alives before connection can be treated as closed. */
    int keepAliveProbeCount;

    Timeouts()
    :
        keepAlivePeriod(std::chrono::hours(2)),
        keepAliveProbeCount(3)
    {
    }

    std::chrono::seconds maxConnectionInactivityPeriod() const
    {
        return keepAlivePeriod*keepAliveProbeCount;
    }
};

/**
 * Creates connections (UDT) after UDP hole punching has been successfully done.
 * Also, makes some efforts to keep UDP hole opened.
 * NOTE: OutgoingTunnelConnection instance
 *     can be safely freed while in aio thread (e.g., in any handler).
 */
class NX_NETWORK_API OutgoingTunnelConnection:
    public AbstractOutgoingTunnelConnection,
    public nx::network::server::StreamConnectionHolder<
		nx::network::server::BaseStreamProtocolConnectionEmbeddable<
			nx::stun::Message,
            nx::stun::MessageParser,
            nx::stun::MessageSerializer>>
{
public:
    /**
     * @param connectionId unique id of connection established.
     * @param udtConnection already established connection to the target host.
     */
    OutgoingTunnelConnection(
        aio::AbstractAioThread* aioThread,
        nx::String connectionId,
        std::unique_ptr<UdtStreamSocket> udtConnection,
        Timeouts timeouts);
    OutgoingTunnelConnection(
        aio::AbstractAioThread* aioThread,
        nx::String connectionId,
        std::unique_ptr<UdtStreamSocket> udtConnection);
    ~OutgoingTunnelConnection();

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;
    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    virtual std::string toString() const override;

private:
    typedef nx::network::server::BaseStreamProtocolConnectionEmbeddable<
        nx::stun::Message,
        nx::stun::MessageParser,
        nx::stun::MessageSerializer
    > ConnectionType;

    struct ConnectionContext
    {
        std::unique_ptr<UdtStreamSocket> connection;
        OnNewConnectionHandler completionHandler;
        nx::network::aio::AbstractAioThread* aioThreadToUse;
    };

    const nx::String m_connectionId;
    const SocketAddress m_localPunchedAddress;
    const SocketAddress m_remoteHostAddress;
    nx::utils::AtomicUniquePtr<ConnectionType> m_controlConnection;
    const Timeouts m_timeouts;
    std::map<UdtStreamSocket*, ConnectionContext> m_ongoingConnections;
    QnMutex m_mutex;
    bool m_pleaseStopHasBeenCalled;
    bool m_pleaseStopCompleted;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>
        m_controlConnectionClosedHandler;

    void proceedWithConnection(
        UdtStreamSocket* connectionPtr,
        std::chrono::milliseconds timeout);
    void onConnectCompleted(
        UdtStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode);
    void reportConnectResult(
        UdtStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode);

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override;
    void onStunMessageReceived(nx::stun::Message message);
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
