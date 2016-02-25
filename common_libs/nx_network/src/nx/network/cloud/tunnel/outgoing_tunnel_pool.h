/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>
#include <map>
#include <memory>

#include <nx/utils/thread/mutex.h>
#include <utils/common/stoppable.h>

#include "nx/network/cloud/address_resolver.h"
#include "outgoing_tunnel.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API OutgoingTunnelPool
:
    public QnStoppableAsync
{
public:
    OutgoingTunnelPool();
    virtual ~OutgoingTunnelPool();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /** Establish new connection.
    * \param timeout zero - no timeout
    * \param socketAttributes attribute values to apply to a newly-created socket
    * \note This method is re-enterable. So, it can be called in
    *        different threads simultaneously */
    void establishNewConnection(
        const AddressEntry& targetHostAddress,
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OutgoingTunnel::NewConnectionHandler handler);

private:
    typedef std::map<QString, std::unique_ptr<OutgoingTunnel>> TunnelDictionary;

    QnMutex m_mutex;
    TunnelDictionary m_pool;
    bool m_terminated;

    const std::unique_ptr<OutgoingTunnel>& 
        getTunnel(const AddressEntry& targetHostAddress);
    void removeTunnel(TunnelDictionary::iterator tunnelIter);
    void tunnelsStopped(nx::utils::MoveOnlyFunc<void()> completionHandler);
};

} // namespace cloud
} // namespace network
} // namespace nx
