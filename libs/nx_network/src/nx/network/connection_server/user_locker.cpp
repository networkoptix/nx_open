// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_locker.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/time.h>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace network {
namespace server {

void UserLockerSettings::load(const SettingsReader& settings)
{
    checkPeriod = nx::utils::parseTimerDuration(
        settings.value("checkPeriod").toString(),
        checkPeriod);

    authFailureCount = settings.value("authFailureCount", authFailureCount).toInt();
    maxLockerCount = settings.value("maxLockerCount", maxLockerCount).toInt();

    lockPeriod = nx::utils::parseTimerDuration(
        settings.value("lockPeriod").toString(),
        lockPeriod);
}

//-------------------------------------------------------------------------------------------------

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

} // namespace server
} // namespace network
} // namespace nx
