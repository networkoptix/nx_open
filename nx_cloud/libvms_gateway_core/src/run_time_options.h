#pragma once

#include <set>

#include <nx/network/socket_common.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

class RunTimeOptions
{
public:
    void enforceSsl(const SocketAddress& address, bool enabled = true);
    bool isSslEnforced(const SocketAddress& address) const;

private:
    mutable QnMutex m_mutex;
    std::set<SocketAddress> m_enforcedSslAddresses;
};

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx
