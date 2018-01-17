#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>

#include <nx/network/aio/timer.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/address_resolver.h>
#include <nx/utils/counter.h>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/subscription.h>

#include "outgoing_tunnel.h"

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API OutgoingTunnelPool:
    public QnStoppableAsync
{
public:
    using OnTunnelClosedSubscription = nx::utils::Subscription<QString>;

    OutgoingTunnelPool();
    virtual ~OutgoingTunnelPool();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * Establish new connection for user needs.
     * @param timeout Zero means no timeout.
     * @param socketAttributes Attribute values to apply to a newly-created socket.
     * NOTE: This method can be called from different threads simultaneously.
     */
    void establishNewConnection(
        const AddressEntry& targetHostAddress,
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        AbstractOutgoingTunnel::NewConnectionHandler handler);

    /**
     * @return Peer id for cloud connect.
     */
    String ownPeerId() const;

    /**
     * Should be called somewhere in every module exactly once, so this id is useful for debug.
     * @param name Short module name, useful for debug.
     * @param uuid Unique instance id, e.g. Hardware id.
     * NOTE: This method generates peer id in internal format using supplied information (name & uuid).
     */
    void assignOwnPeerId(const String& name, const QnUuid& uuid);
    /**
     * NOTE: This method just sets already prepared peerId.
     *   E.g., received from OutgoingTunnelPool::ownPeerId().
     */
    void setOwnPeerId(const String& peerId);
    void clearOwnPeerIdIfEqual(const String& name, const QnUuid& uuid);

    OnTunnelClosedSubscription& onTunnelClosedSubscription();

    // TODO: Remove this function when SocketGlobals are not dependent on cloud any more.
    /** Unit test usage only! */
    static void ignoreOwnPeerIdChange();

private:
    struct TunnelContext
    {
        std::unique_ptr<AbstractOutgoingTunnel> tunnel;
        std::list<AbstractOutgoingTunnel::NewConnectionHandler> handlers;
    };

    typedef std::map<QString, std::unique_ptr<TunnelContext>> TunnelDictionary;

    mutable QnMutex m_mutex;
    mutable bool m_isOwnPeerIdAssigned;
    String m_ownPeerId;
    TunnelDictionary m_pool;
    bool m_terminated;
    bool m_stopping;
    aio::Timer m_aioThreadBinder;
    OnTunnelClosedSubscription m_onTunnelClosedSubscription;
    nx::utils::Counter m_counter;

    TunnelContext& getTunnel(const AddressEntry& targetHostAddress);
    void reportConnectionResult(
        SystemError::ErrorCode sysErrorCode,
        TunnelAttributes tunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection,
        TunnelContext* tunnelContext,
        std::list<AbstractOutgoingTunnel::NewConnectionHandler>::iterator handlerIter);
    void onTunnelClosed(AbstractOutgoingTunnel* tunnelPtr);
    void tunnelsStopped(nx::utils::MoveOnlyFunc<void()> completionHandler);

    static bool s_isIgnoringOwnPeerIdChange;
};

} // namespace cloud
} // namespace network
} // namespace nx
