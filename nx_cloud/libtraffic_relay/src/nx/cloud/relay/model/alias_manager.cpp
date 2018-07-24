#include "alias_manager.h"

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

namespace nx::cloud::relay::model {

AliasManager::AliasManager(
    std::chrono::milliseconds unusedAliasExpirationPeriod)
    :
    m_unusedAliasExpirationPeriod(unusedAliasExpirationPeriod),
    m_timers(std::bind(&AliasManager::removeExpiredAlias, this, std::placeholders::_1))
{
}

std::string AliasManager::addAliasToHost(const std::string& hostName)
{
    constexpr int kAliasLength = 7;

    QnMutexLocker locker(&m_mutex);

    m_timers.processTimers();

    for (;;)
    {
        auto alias = nx::utils::generateRandomName(kAliasLength).toLower().toStdString();
        if (!m_aliasToHostName.emplace(alias, hostName).second)
            continue; //< Alias collision. Trying once more.

        m_timers.addTimer(alias, m_unusedAliasExpirationPeriod);
        NX_VERBOSE(this, lm("Added alias %1 to host %2").args(alias, hostName));
        return alias;
    }
}

std::optional<std::string> AliasManager::findHostByAlias(const std::string& alias) const
{
    QnMutexLocker locker(&m_mutex);

    m_timers.processTimers();

    auto it = m_aliasToHostName.find(alias);
    if (it == m_aliasToHostName.end())
        return std::nullopt;
    m_timers.addTimer(alias, m_unusedAliasExpirationPeriod);
    return it->second;
}

void AliasManager::updateAlias(const std::string& alias, const std::string& hostName)
{
    QnMutexLocker locker(&m_mutex);

    m_timers.processTimers();

    m_aliasToHostName[alias] = hostName;
    m_timers.addTimer(alias, m_unusedAliasExpirationPeriod);
    NX_VERBOSE(this, lm("Updated alias %1 to host %2").args(alias, hostName));
}

void AliasManager::removeExpiredAlias(const std::string& alias)
{
    m_aliasToHostName.erase(alias);
}

} // namespace nx::cloud::relay::model
