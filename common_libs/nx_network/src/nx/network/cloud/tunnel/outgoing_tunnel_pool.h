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

class OutgoingTunnelPool
:
    public QnStoppableAsync
{
public:
    virtual ~OutgoingTunnelPool();

    virtual void pleaseStop(std::function<void()> completionHandler) override;

    std::shared_ptr<OutgoingTunnel> getTunnel(const AddressEntry& targetHostAddress);

private:
    typedef std::map<QString, std::shared_ptr<OutgoingTunnel>> TunnelDictionary;

    QnMutex m_mutex;
    TunnelDictionary m_pool;

    void removeTunnel(TunnelDictionary::iterator tunnelIter);
};

} // namespace cloud
} // namespace network
} // namespace nx
