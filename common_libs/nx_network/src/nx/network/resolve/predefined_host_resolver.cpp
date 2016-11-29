#include "predefined_host_resolver.h"

namespace nx {
namespace network {

std::deque<HostAddress> PredefinedHostResolver::resolve(const QString& name, int ipVersion)
{
    QnMutexLocker lock(&m_mutex);

    const auto it = m_etcHosts.find(name);
    if (it == m_etcHosts.end())
        return std::deque<HostAddress>();

    std::deque<HostAddress> ipAddresses;
    for (const auto address : it->second)
    {
        if (ipVersion != AF_INET || address.ipV4())
            ipAddresses.push_back(address);
    }

    return ipAddresses;
}

void PredefinedHostResolver::addEtcHost(const QString& name, std::vector<HostAddress> addresses)
{
    QnMutexLocker lock(&m_mutex);
    m_etcHosts[name] = std::move(addresses);
}

void PredefinedHostResolver::removeEtcHost(const QString& name)
{
    QnMutexLocker lock(&m_mutex);
    m_etcHosts.erase(name);
}

} // namespace network
} // namespace nx
