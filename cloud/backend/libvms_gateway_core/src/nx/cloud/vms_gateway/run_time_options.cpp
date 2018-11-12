#include "run_time_options.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

void RunTimeOptions::enforceSsl(const network::SocketAddress& address, bool enabled)
{
    QnMutexLocker lock(&m_mutex);
    if (enabled)
        m_enforcedSslAddresses.insert(address);
    else
        m_enforcedSslAddresses.erase(address);
}

bool RunTimeOptions::isSslEnforced(const network::SocketAddress& address) const
{
    QnMutexLocker lock(&m_mutex);
    return m_enforcedSslAddresses.find(address) != m_enforcedSslAddresses.end();
}

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx
