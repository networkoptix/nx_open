#include "predefined_host_resolver.h"

#include <boost/algorithm/string.hpp>

#include <nx/utils/std/algorithm.h>

namespace nx {
namespace network {

SystemError::ErrorCode PredefinedHostResolver::resolve(
    const QString& name,
    int ipVersion,
    std::deque<AddressEntry>* resolvedAddresses)
{
    auto reversedName = nx::utils::reverseWords(name.toStdString(), ".");
    nx::utils::to_lower(&reversedName);

    QnMutexLocker lock(&m_mutex);

    // Searching for a smallest string that has reversedName as a prefix.
    const auto it = m_etcHosts.lower_bound(reversedName);
    if (it == m_etcHosts.end() || !boost::starts_with(it->first, reversedName))
        return SystemError::hostNotFound;

    if (reversedName != it->first &&
        !(reversedName.size() < it->first.size() && it->first[reversedName.size()] == '.'))
    {
        // Suitable name must start with "originalName."
        return SystemError::hostNotFound;
    }

    for (const auto entry: it->second)
    {
        if (ipVersion == AF_INET && !entry.host.ipV4())
            continue; //< Only ipv4 hosts are requested.
        resolvedAddresses->push_back(entry);
    }

    return resolvedAddresses->empty()
        ? SystemError::hostNotFound
        : SystemError::noError;
}

void PredefinedHostResolver::addMapping(
    const std::string& name,
    std::deque<AddressEntry> entries)
{
    auto reversedName = nx::utils::reverseWords(name, ".");
    nx::utils::to_lower(&reversedName);

    QnMutexLocker lock(&m_mutex);
    auto& existingEntries = m_etcHosts[reversedName];
    for (auto& entry: entries)
    {
        if (!nx::utils::contains(existingEntries.begin(), existingEntries.end(), entry))
            existingEntries.push_back(std::move(entry));
    }
}

void PredefinedHostResolver::replaceMapping(
    const std::string& name,
    std::deque<AddressEntry> entries)
{
    auto reversedName = nx::utils::reverseWords(name, ".");
    nx::utils::to_lower(&reversedName);

    QnMutexLocker lock(&m_mutex);
    m_etcHosts[reversedName] = std::move(entries);
}

void PredefinedHostResolver::removeMapping(const std::string& name)
{
    auto reversedName = nx::utils::reverseWords(name, ".");
    nx::utils::to_lower(&reversedName);

    QnMutexLocker lock(&m_mutex);
    m_etcHosts.erase(reversedName);
}

void PredefinedHostResolver::removeMapping(
    const std::string& name,
    const AddressEntry& entryToRemove)
{
    auto reversedName = nx::utils::reverseWords(name, ".");
    nx::utils::to_lower(&reversedName);

    QnMutexLocker lock(&m_mutex);
    auto it = m_etcHosts.find(reversedName);
    if (it == m_etcHosts.end())
        return;

    it->second.erase(
        std::remove(it->second.begin(), it->second.end(), entryToRemove),
        it->second.end());
}

} // namespace network
} // namespace nx
