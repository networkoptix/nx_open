#include "predefined_host_resolver.h"

namespace nx {
namespace network {

SystemError::ErrorCode PredefinedHostResolver::resolve(
    const QString& name,
    int ipVersion,
    std::deque<AddressEntry>* resolvedAddresses)
{
    QnMutexLocker lock(&m_mutex);

    const auto it = m_etcHosts.find(name);
    if (it == m_etcHosts.end())
        return SystemError::hostNotFound;

    for (const auto entry: it->second)
    {
        if (ipVersion != AF_INET || entry.host.ipV4()) // TODO: #ak Totally unclear condition.
            resolvedAddresses->push_back(entry);
    }

    return SystemError::noError;
}

void PredefinedHostResolver::addEtcHost(
    const QString& name,
    std::vector<AddressEntry> entries)
{
    QnMutexLocker lock(&m_mutex);
    m_etcHosts[name] = std::move(entries);
}

void PredefinedHostResolver::removeEtcHost(const QString& name)
{
    QnMutexLocker lock(&m_mutex);
    m_etcHosts.erase(name);
}

} // namespace network
} // namespace nx
