/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>

#include <nx/utils/thread/mutex.h>

#include <utils/common/stoppable.h>
#include <utils/common/subscription.h>

#include "nx/network/aio/timer.h"
#include "nx/network/cloud/address_resolver.h"
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
     * @note This method can be called from different threads simultaneously.
     */
    void establishNewConnection(
        const AddressEntry& targetHostAddress,
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OutgoingTunnel::NewConnectionHandler handler);

    /**
     * @return Peer id for cloud connect.
     */
    String ownPeerId() const;

    /**
     * Should be called somewhere in every module exactly once, so this id is useful for debug.
     * @param name Short module name, useful for debug.
     * @param uuid Unique instance id, e.g. Hardware id.
     */
    void assignOwnPeerId(const String& name, const QnUuid& uuid);
    void clearOwnPeerId();

    OnTunnelClosedSubscription& onTunnelClosedSubscription();

private:
    struct TunnelContext
    {
        std::unique_ptr<OutgoingTunnel> tunnel;
        std::list<OutgoingTunnel::NewConnectionHandler> handlers;
    };

    typedef std::map<QString, TunnelContext> TunnelDictionary;

    mutable QnMutex m_mutex;
    mutable bool m_isOwnPeerIdAssigned;
    String m_ownPeerId;
    TunnelDictionary m_pool;
    bool m_terminated;
    bool m_stopping;
    aio::Timer m_aioThreadBinder;
    OnTunnelClosedSubscription m_onTunnelClosedSubscription;

    TunnelContext& getTunnel(const AddressEntry& targetHostAddress);
    void reportConnectionResult(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection,
        TunnelContext* tunnelContext,
        std::list<OutgoingTunnel::NewConnectionHandler>::iterator handlerIter);
    void onTunnelClosed(OutgoingTunnel* tunnelPtr);
    void tunnelsStopped(nx::utils::MoveOnlyFunc<void()> completionHandler);
};

} // namespace cloud
} // namespace network
} // namespace nx
