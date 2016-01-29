
#pragma once

#include <utils/common/stoppable.h>

#include "tunnel.h"


namespace nx {
namespace network {
namespace cloud {

class OutgoingTunnel
:
    public Tunnel
{
public:
    OutgoingTunnel(String remotePeerId);

    /** Establish new connection
    * \param socketAttributes attribute values to apply to a newly-created socket
    * \note This method is re-enterable. So, it can be called in
    *        different threads simultaneously */
    void connect(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        SocketHandler handler);
};

class OutgoingTunnelPool
:
    public QnStoppableAsync
{
public:
    virtual void pleaseStop(std::function<void()> handler) override;

    std::shared_ptr<OutgoingTunnel> getTunnel(const String& hostName);

private:
    QnMutex m_mutex;
    std::map<String, std::shared_ptr<OutgoingTunnel>> m_pool;

    void removeTunnel(const String& peerId);
};

} // namespace cloud
} // namespace network
} // namespace nx
