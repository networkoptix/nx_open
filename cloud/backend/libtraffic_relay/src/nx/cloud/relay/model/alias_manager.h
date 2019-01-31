#pragma once

#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

namespace nx::cloud::relay::model {

class AliasManager
{
public:
    AliasManager(std::chrono::milliseconds unusedAliasExpirationPeriod);

    /**
     * @return Alias.
     */
    std::string addAliasToHost(const std::string& hostName);

    std::optional<std::string> findHostByAlias(const std::string& alias) const;

    void updateAlias(const std::string& alias, const std::string& hostName);

private:
    mutable QnMutex m_mutex;
    std::map<std::string, std::string> m_aliasToHostName;
    const std::chrono::milliseconds m_unusedAliasExpirationPeriod;
    mutable nx::utils::ElapsedTimerPool<std::string /*alias*/> m_timers;

    void removeExpiredAlias(const std::string& alias);
};

} // namespace nx::cloud::relay::model
