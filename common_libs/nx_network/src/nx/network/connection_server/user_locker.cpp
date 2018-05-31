#include "user_locker.h"

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

void UserLocker::updateLockoutState(AuthResult authResult)
{
    const auto now = std::chrono::steady_clock::now();

    // If there is an expired lock, resetting.
    if (m_userLockedUntil && *m_userLockedUntil <= now)
    {
        m_userLockedUntil = std::nullopt;
        m_failedAuthCountCalculator.reset();
    }

    if (authResult == AuthResult::failure)
    {
        m_failedAuthCountCalculator.add(1);
        if (m_failedAuthCountCalculator.getSumPerLastPeriod() >= m_settings.authFailureCount)
            m_userLockedUntil = now + m_settings.lockPeriod;
    }
    else
    {
        m_failedAuthCountCalculator.reset();
    }
}

bool UserLocker::isLocked() const
{
    return m_userLockedUntil && (*m_userLockedUntil > std::chrono::steady_clock::now());
}

//-------------------------------------------------------------------------------------------------

UserLockerPool::UserLockerPool(const UserLockerSettings& settings):
    m_settings(settings)
{
}

void UserLockerPool::updateLockoutState(
    const Key& key,
    UserLocker::AuthResult authResult)
{
    QnMutexLocker lock(&m_mutex);
    const auto iterAndInsertedFlag = m_userLockers.try_emplace(key, m_settings);
    iterAndInsertedFlag.first->second.updateLockoutState(authResult);
}

bool UserLockerPool::isLocked(const Key& key) const
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_userLockers.find(key);
    return it == m_userLockers.end() ? false : it->second.isLocked();
}

} // namespace server
} // namespace network
} // namespace nx
