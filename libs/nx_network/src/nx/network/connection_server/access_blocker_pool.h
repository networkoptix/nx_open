// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/thread/mutex.h>

namespace nx::network::server {

enum class AuthResult
{
    success,
    failure,
};

NX_NETWORK_API std::string toString(AuthResult authResult);

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

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams = void>
class BaseAccessBlockerPool
{
public:
    using key_type = Key;

    using LockUpdateHistoryRecord = GenericLockUpdateHistoryRecord<Key, UpdateHistoryParams>;

    BaseAccessBlockerPool(
        const Settings& settings,
        std::chrono::milliseconds unusedLockerExpirationPeriod,
        int maxLockerCount);

    bool isLocked(const Key& key) const;

    std::vector<Key> getLockedKeys() const;

    std::vector<LockUpdateHistoryRecord> getCurrentLockReason(const Key& key) const;

    bool lockerExists(const Key& key) const;

    const Settings& settings() const;

protected:
    mutable nx::Mutex m_mutex;

    bool isLocked(const::nx::Locker<nx::Mutex>& /*lock*/, const Key& key) const;

    template<typename... Args>
    LockUpdateResult updateLockoutState(
        const nx::Locker<nx::Mutex>& lock,
        const Key& key,
        AuthResult authResult,
        Args&&... args);

    void saveLockUpdateHistoryRecord(
        const nx::Locker<nx::Mutex>& lock,
        LockUpdateHistoryRecord record);

private:
    struct LockerContext
    {
        std::unique_ptr<Locker> locker;
        std::deque<LockUpdateHistoryRecord> history;
    };

    const Settings m_settings;
    nx::utils::TimeOutCache<Key, LockerContext, std::map> m_userLockers;
    const std::chrono::milliseconds m_unusedLockerExpirationPeriod;
};

//-------------------------------------------------------------------------------------------------

/**
 * Manages pool of Locker objects. Each Locker instance is identified by a key.
 * Lockers are kept in a LRU cache internally.
 * maxLockerCount constructor argument specifies the size of the cache.
 * NOTE: Thread safe.
 */
template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams = void>
class AccessBlockerPool:
    public BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>
{
    using base_type = BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>;

public:
    using base_type::base_type;

    template<typename... Args>
    LockUpdateResult updateLockoutState(
        const Key& key,
        AuthResult authResult,
        UpdateHistoryParams updateHistoryParams,
        Args&&... args)
    {
        NX_MUTEX_LOCKER lock(&this->m_mutex);

        const auto result = this->base_type::updateLockoutState(
            lock, key, authResult, std::forward<decltype(args)>(args)...);

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

    template<typename... Args>
    LockUpdateResult updateLockoutState(
        const Key& key,
        AuthResult authResult,
        Args&&... args)
    {
        NX_MUTEX_LOCKER lock(&this->m_mutex);

        const auto result = this->base_type::updateLockoutState(
            lock, key, authResult, std::forward<decltype(args)>(args)...);

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
    std::chrono::milliseconds unusedLockerExpirationPeriod,
    int maxLockerCount)
    :
    m_settings(settings),
    m_userLockers(unusedLockerExpirationPeriod, maxLockerCount),
    m_unusedLockerExpirationPeriod(unusedLockerExpirationPeriod)
{
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
bool BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::isLocked(
    const::nx::Locker<nx::Mutex>& /*lock*/,
    const Key& key) const
{
    auto val = m_userLockers.peekValue(key);
    return val ? val->get().locker->isLocked() : false;
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
template<typename ... Args>
LockUpdateResult BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::updateLockoutState(
    const nx::Locker<nx::Mutex>& /*lock*/,
    const Key& key,
    AuthResult authResult,
    Args&&... args)
{
    auto val = m_userLockers.getValue(key);
    const auto exists = val != std::nullopt;

    if (authResult == AuthResult::success && !exists)
        return LockUpdateResult::noChange;

    if (!exists)
    {
        LockerContext context;
        context.locker = std::make_unique<Locker>(m_settings);
        m_userLockers.put(key, std::move(context));
        val = m_userLockers.getValue(key);
    }

    const auto result = val->get().locker->updateLockoutState(
        authResult, std::forward<decltype(args)>(args)...);

    if (authResult == AuthResult::success && !val->get().locker->isLocked())
        m_userLockers.erase(key);

    return result;
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
void BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::saveLockUpdateHistoryRecord(
    const nx::Locker<nx::Mutex>& /*lock*/,
    LockUpdateHistoryRecord record)
{
    static constexpr int kHistoryRecordsToKeep = 5;

    auto val = m_userLockers.getValue(record.key);
    if (!val)
        return;

    val->get().history.push_back(std::move(record));
    while (val->get().history.size() > kHistoryRecordsToKeep)
        val->get().history.pop_front();
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
bool BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::isLocked(
    const Key& key) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return isLocked(lock, key);
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
inline std::vector<Key> BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::getLockedKeys() const
{
    std::vector<Key> lockedKeys;

    NX_MUTEX_LOCKER lock(&m_mutex);

    for (const auto& keyAndValue: m_userLockers)
    {
        if (isLocked(lock, keyAndValue.first))
            lockedKeys.push_back(keyAndValue.first);
    }

    return lockedKeys;
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
std::vector<typename BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::LockUpdateHistoryRecord>
    BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::getCurrentLockReason(
        const Key& key) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto val = m_userLockers.peekValue(key);
    if (!val)
        return {};

    std::vector<LockUpdateHistoryRecord> result(
        val->get().history.begin(), val->get().history.end());
    return result;
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
bool BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::lockerExists(
    const Key& key) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_userLockers.contains(key);
}

template<typename Key, typename Locker, typename Settings, typename UpdateHistoryParams>
const Settings& BaseAccessBlockerPool<Key, Locker, Settings, UpdateHistoryParams>::settings() const
{
    return m_settings;
}

} // namespace nx::network::server
