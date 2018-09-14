#pragma once

#include <chrono>
#include <map>

#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/thread/mutex.h>

namespace nx::network::server {

enum class AuthResult
{
    success,
    failure,
};

enum class LockUpdateResult
{
    noChange,
    locked,
    unlocked,
};

/**
 * Manages pool of Locker objects. Each Locker instance is identified by a key.
 * NOTE: Thread safe.
 */
template<typename Key, typename Locker, typename Settings>
class AccessBlockerPool
{
public:
    using key_type = Key;

    AccessBlockerPool(
        const Settings& settings,
        std::chrono::milliseconds unusedLockerExpirationPeriod);

    template<typename ... Args>
    LockUpdateResult updateLockoutState(
        const Key& key,
        AuthResult authResult,
        Args... args);
    bool isLocked(const Key& key) const;

    std::map<Key, Locker> userLockers() const;

    const Settings& settings() const;

private:
    const Settings m_settings;
    mutable QnMutex m_mutex;
    std::map<Key, Locker> m_userLockers;
    mutable utils::ElapsedTimerPool<Key> m_timers;
    const std::chrono::milliseconds m_unusedLockerExpirationPeriod;

    void removeLocker(const Key& key);
};

//-------------------------------------------------------------------------------------------------

template<typename Key, typename Locker, typename Settings>
AccessBlockerPool<Key, Locker, Settings>::AccessBlockerPool(
    const Settings& settings,
    std::chrono::milliseconds unusedLockerExpirationPeriod)
    :
    m_settings(settings),
    m_timers(std::bind(&AccessBlockerPool::removeLocker, this, std::placeholders::_1)),
    m_unusedLockerExpirationPeriod(unusedLockerExpirationPeriod)
{
}

template<typename Key, typename Locker, typename Settings>
template<typename ... Args>
LockUpdateResult AccessBlockerPool<Key, Locker, Settings>::updateLockoutState(
    const Key& key,
    AuthResult authResult,
    Args... args)
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    auto it = m_userLockers.lower_bound(key);
    const auto exists = it != m_userLockers.end() && it->first == key;

    if (authResult == AuthResult::success && !exists)
        return LockUpdateResult::noChange;

    if (!exists)
        it = m_userLockers.emplace_hint(it, key, m_settings);

    const auto result = it->second.updateLockoutState(authResult, args...);

    if (authResult == AuthResult::success && !it->second.isLocked())
    {
        m_userLockers.erase(it);
        m_timers.removeTimer(key);
    }
    else
    {
        m_timers.addTimer(key, m_unusedLockerExpirationPeriod);
    }

    return result;
}

template<typename Key, typename Locker, typename Settings>
bool AccessBlockerPool<Key, Locker, Settings>::isLocked(const Key& key) const
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    auto it = m_userLockers.find(key);
    return it == m_userLockers.end() ? false : it->second.isLocked();
}

template<typename Key, typename Locker, typename Settings>
std::map<Key, Locker> AccessBlockerPool<Key, Locker, Settings>::userLockers() const
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    return m_userLockers;
}

template<typename Key, typename Locker, typename Settings>
const Settings& AccessBlockerPool<Key, Locker, Settings>::settings() const
{
    return m_settings;
}

template<typename Key, typename Locker, typename Settings>
void AccessBlockerPool<Key, Locker, Settings>::removeLocker(const Key& key)
{
    m_userLockers.erase(key);
}

} // namespace nx::network::server
