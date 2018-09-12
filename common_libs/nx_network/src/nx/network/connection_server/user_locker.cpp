#include "user_locker.h"

#include <nx/utils/time.h>

namespace nx {
namespace network {
namespace server {

UserLocker::UserLocker(
    const UserLockerSettings& settings)
    :
    m_settings(settings),
    m_failedAuthCountCalculator(m_settings.checkPeriod)
{
}

LockUpdateResult UserLocker::updateLockoutState(AuthResult authResult)
{
    const auto now = nx::utils::monotonicTime();
    
    LockUpdateResult result = LockUpdateResult::noChange;

    // If there is an expired lock, resetting.
    if (m_userLockedUntil && *m_userLockedUntil <= now)
    {
        m_userLockedUntil = std::nullopt;
        m_failedAuthCountCalculator.reset();
        result = LockUpdateResult::unlocked;
    }

    if (authResult == AuthResult::failure)
    {
        m_failedAuthCountCalculator.add(1);
        if (m_failedAuthCountCalculator.getSumPerLastPeriod() >= m_settings.authFailureCount)
        {
            m_userLockedUntil = now + m_settings.lockPeriod;
            result = LockUpdateResult::locked;
        }
    }
    else
    {
        m_failedAuthCountCalculator.reset();
    }

    return result;
}

bool UserLocker::isLocked() const
{
    return m_userLockedUntil
        && (*m_userLockedUntil > nx::utils::monotonicTime());
}

//-------------------------------------------------------------------------------------------------

UserLockerPool::UserLockerPool(const UserLockerSettings& settings):
    m_settings(settings),
    m_timers(std::bind(&UserLockerPool::removeLocker, this, std::placeholders::_1)),
    m_unusedLockerExpirationPeriod(std::max(m_settings.checkPeriod, m_settings.lockPeriod))
{
}

LockUpdateResult UserLockerPool::updateLockoutState(
    const Key& key,
    UserLocker::AuthResult authResult)
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    auto it = m_userLockers.lower_bound(key);
    const auto exists = it != m_userLockers.end() && it->first == key;

    if (authResult == UserLocker::AuthResult::success && !exists)
        return LockUpdateResult::noChange;

    if (!exists)
        it = m_userLockers.emplace_hint(it, key, m_settings);

    const auto result = it->second.updateLockoutState(authResult);

    if (authResult == UserLocker::AuthResult::success && !it->second.isLocked())
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

bool UserLockerPool::isLocked(const Key& key) const
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    auto it = m_userLockers.find(key);
    return it == m_userLockers.end() ? false : it->second.isLocked();
}

std::map<UserLockerPool::Key, UserLocker> UserLockerPool::userLockers() const
{
    QnMutexLocker lock(&m_mutex);

    m_timers.processTimers();

    return m_userLockers;
}

const UserLockerSettings& UserLockerPool::settings() const
{
    return m_settings;
}

void UserLockerPool::removeLocker(const Key& key)
{
    m_userLockers.erase(key);
}

} // namespace server
} // namespace network
} // namespace nx
