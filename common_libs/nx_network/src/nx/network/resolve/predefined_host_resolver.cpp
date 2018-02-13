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
        if (ipVersion == AF_INET && !entry.host.ipV4())
            continue; //< Only ipv4 hosts are requested.
        resolvedAddresses->push_back(entry);
    }

    return SystemError::noError;
}

void PredefinedHostResolver::addMapping(
    const QString& name,
    std::deque<AddressEntry> entries)
{
    QnMutexLocker lock(&m_mutex);
    m_etcHosts[name] = std::move(entries);
}

void PredefinedHostResolver::removeMapping(const QString& name)
{
    QnMutexLocker lock(&m_mutex);
    m_etcHosts.erase(name);
}

void PredefinedHostResolver::removeMapping(
    const QString& name,
    const AddressEntry& entryToRemove)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_etcHosts.find(name);
    if (it == m_etcHosts.end())
        return;

    it->second.erase(
        std::remove(it->second.begin(), it->second.end(), entryToRemove),
        it->second.end());
}

} // namespace network
} // namespace nx
