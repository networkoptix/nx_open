#include "predefined_host_resolver.h"

namespace nx {
namespace network {

SystemError::ErrorCode PredefinedHostResolver::resolve(
    const QString& name,
    int ipVersion,
    std::deque<HostAddress>* resolvedAddresses)
{
    QnMutexLocker lock(&m_mutex);

    const auto it = m_etcHosts.find(name);
    if (it == m_etcHosts.end())
        return SystemError::hostNotFound;

    for (const auto address : it->second)
    {
        if (ipVersion != AF_INET || address.ipV4())
            resolvedAddresses->push_back(address);
    }

    return SystemError::noError;
}

void PredefinedHostResolver::addEtcHost(
    const QString& name,
    std::vector<HostAddress> addresses)
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
