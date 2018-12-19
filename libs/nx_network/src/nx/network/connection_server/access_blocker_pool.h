#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <vector>
#include <type_traits>

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

template<typename Key, typename Params>
struct GenericLockUpdateHistoryRecord
{
    std::chrono::system_clock::time_point timestamp;
    Key key;
    Params params;
};

template<typename Key>
struct GenericLockUpdateHistoryRecord<Key, void>
{
    std::chrono::system_clock::time_point timestamp;
    Key key;
};

/**
 * Manages pool of Locker objects. Each Locker instance is identified by a key.
 * NOTE: Thread safe.
 */
template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams = void>
class BaseAccessBlockerPool
{
public:
    using key_type = Key;

    using LockUpdateHistoryRecord = GenericLockUpdateHistoryRecord<Key, UpdateHistoryParams>;

    BaseAccessBlockerPool(
        const Settings& settings,
        std::chrono::milliseconds unusedLockerExpirationPeriod);

    bool isLocked(const Key& key) const;

    std::vector<LockUpdateHistoryRecord> getCurrentLockReason(const Key& key) const;

    bool lockerExists(const Key& key) const;

    const Settings& settings() const;

protected:
    mutable QnMutex m_mutex;

    template<typename ... Args>
    LockUpdateResult updateLockoutState(
        const QnMutexLockerBase& lock,
        const Key& key,
        AuthResult authResult,
        Args... args);

    void saveLockUpdateHistoryRecord(
        const QnMutexLockerBase& lock,
        LockUpdateHistoryRecord record);

private:
    struct LockerContext
    {
        std::unique_ptr<Locker> locker;
        std::deque<LockUpdateHistoryRecord> history;
    };

    const Settings m_settings;
    std::map<Key, LockerContext> m_userLockers;
    mutable utils::ElapsedTimerPool<Key> m_timers;
    const std::chrono::milliseconds m_unusedLockerExpirationPeriod;

    void removeLocker(const Key& key);
};

//-------------------------------------------------------------------------------------------------

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams = void>
class AccessBlockerPool:
    public BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>
{
    using base_type = BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>;

public:
    using base_type::base_type;

    template<typename ... Args>
    LockUpdateResult updateLockoutState(
        const Key& key,
        AuthResult authResult,
        UpdateHistoryParams updateHistoryParams,
        Args... args)
    {
        QnMutexLocker lock(&this->m_mutex);

        const auto result = this->base_type::updateLockoutState(
            lock, key, authResult, std::move(args)...);

        if (authResult == AuthResult::success)
            return result;

        typename base_type::LockUpdateHistoryRecord lockUpdateHistoryRecord;
        lockUpdateHistoryRecord.timestamp = nx::utils::utcTime();
        lockUpdateHistoryRecord.key = key;
        lockUpdateHistoryRecord.params = std::move(updateHistoryParams);
        this->saveLockUpdateHistoryRecord(lock, std::move(lockUpdateHistoryRecord));

        return result;
    }
};

//-------------------------------------------------------------------------------------------------

template<typename Key, typename Locker, typename Settings>
class AccessBlockerPool<Key, Locker, Settings, void>:
    public BaseAccessBlockerPool<Key, Locker, Settings, void>
{
    using base_type = BaseAccessBlockerPool<Key, Locker, Settings, void>;

public:
    using base_type::base_type;

    template<typename ... Args>
    LockUpdateResult updateLockoutState(
        const Key& key,
        AuthResult authResult,
        Args... args)
    {
        QnMutexLocker lock(&this->m_mutex);

        const auto result = this->base_type::updateLockoutState(
            lock, key, authResult, std::move(args)...);

        if (authResult == AuthResult::success)
            return result;

        typename base_type::LockUpdateHistoryRecord lockUpdateHistoryRecord;
        lockUpdateHistoryRecord.timestamp = nx::utils::utcTime();
        lockUpdateHistoryRecord.key = key;
        this->saveLockUpdateHistoryRecord(lock, std::move(lockUpdateHistoryRecord));

        return result;
    }
};

//-------------------------------------------------------------------------------------------------

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::BaseAccessBlockerPool(
    const Settings& settings,
    std::chrono::milliseconds unusedLockerExpirationPeriod)
    :
    m_settings(settings),
    m_timers(std::bind(&BaseAccessBlockerPool::removeLocker, this, std::placeholders::_1)),
    m_unusedLockerExpirationPeriod(unusedLockerExpirationPeriod)
{
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
template<typename ... Args>
LockUpdateResult BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::updateLockoutState(
    const QnMutexLockerBase& /*lock*/,
    const Key& key,
    AuthResult authResult,
    Args... args)
{
    m_timers.processTimers();

    auto it = m_userLockers.lower_bound(key);
    const auto exists = it != m_userLockers.end() && it->first == key;

    if (authResult == AuthResult::success && !exists)
        return LockUpdateResult::noChange;

    if (!exists)
    {
        LockerContext context;
        context.locker = std::make_unique<Locker>(m_settings);
        it = m_userLockers.emplace_hint(it, key, std::move(context));
    }

    const auto result = it->second.locker->updateLockoutState(authResult, std::move(args)...);

    if (authResult == AuthResult::success && !it->second.locker->isLocked())
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

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
void BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::saveLockUpdateHistoryRecord(
    const QnMutexLockerBase& /*lock*/,
    LockUpdateHistoryRecord record)
{
    static constexpr int kHistoryRecordsToKeep = 5;

    auto it = m_userLockers.find(record.key);
    if (it == m_userLockers.end())
        return;

    it->second.history.push_back(std::move(record));
    while (it->second.history.size() > kHistoryRecordsToKeep)
        it->second.history.pop_front();
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
bool BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::isLocked(
    const Key& key) const
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    auto it = m_userLockers.find(key);
    return it == m_userLockers.end() ? false : it->second.locker->isLocked();
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
std::vector<typename BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::LockUpdateHistoryRecord>
    BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::getCurrentLockReason(
        const Key& key) const
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_userLockers.find(key);
    if (it == m_userLockers.end())
        return {};

    std::vector<LockUpdateHistoryRecord> result(
        it->second.history.begin(), it->second.history.end());
    return result;
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
bool BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::lockerExists(
    const Key& key) const
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    return m_userLockers.find(key) != m_userLockers.end();
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
const Settings& BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::settings() const
{
    return m_settings;
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
void BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::removeLocker(const Key& key)
{
    m_userLockers.erase(key);
}

} // namespace nx::network::server
