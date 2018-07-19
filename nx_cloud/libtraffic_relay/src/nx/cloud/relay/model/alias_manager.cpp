#include "alias_manager.h"

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

namespace nx::cloud::relay::model {

std::string AliasManager::addAliasToHost(const std::string& hostName)
{
    constexpr int kAliasLength = 7;

    QnMutexLocker locker(&m_mutex);
    for (;;)
    {
        auto alias = nx::utils::generateRandomName(kAliasLength).toLower().toStdString();
        if (!m_aliasToHostName.emplace(alias, hostName).second)
            continue; //< Alias collision. Trying once more.

        NX_VERBOSE(this, lm("Added alias %1 to host %2").args(alias, hostName));
        return alias;
    }
}

std::optional<std::string> AliasManager::findHostByAlias(const std::string& alias) const
{
    QnMutexLocker locker(&m_mutex);
    auto it = m_aliasToHostName.find(alias);
    if (it == m_aliasToHostName.end())
        return std::nullopt;
    return it->second;
}

void AliasManager::updateAlias(const std::string& alias, const std::string& hostName)
{
    QnMutexLocker locker(&m_mutex);
    m_aliasToHostName[alias] = hostName;
    NX_VERBOSE(this, lm("Updated alias %1 to host %2").args(alias, hostName));
}

} // namespace nx::cloud::relay::model
