#include "login_enumeration_protector.h"

namespace nx::cdb {

LoginEnumerationProtector::LoginEnumerationProtector(
    const LoginEnumerationProtectionSettings& settings)
    :
    m_settings(settings),
    m_authenticatedLoginsPerPeriod(settings.period),
    m_unauthenticatedLoginsPerPeriod(settings.period)
{
}

nx::network::server::LockUpdateResult LoginEnumerationProtector::updateLockoutState(
    nx::network::server::AuthResult authResult,
    const std::string& login)
{
    auto result = nx::network::server::LockUpdateResult::noChange;

    if (removeExpiredLock())
        result = nx::network::server::LockUpdateResult::unlocked;

    if (isLocked())
        return nx::network::server::LockUpdateResult::noChange;

    updateCounters(authResult, login);

    setLockIfAppropriate();
    if (isLocked())
        result = nx::network::server::LockUpdateResult::locked;

    return result;
}

bool LoginEnumerationProtector::isLocked() const
{
    const_cast<LoginEnumerationProtector*>(this)->removeExpiredLock();

    return static_cast<bool>(m_lockExpiration);
}

bool LoginEnumerationProtector::removeExpiredLock()
{
    if (m_lockExpiration && *m_lockExpiration < nx::utils::monotonicTime())
    {
        m_lockExpiration.reset();
        return true;
    }

    return false;
}

void LoginEnumerationProtector::updateCounters(
    nx::network::server::AuthResult authResult,
    const std::string& login)
{
    if (authResult == nx::network::server::AuthResult::success)
        m_authenticatedLoginsPerPeriod.add(login);
    else
        m_unauthenticatedLoginsPerPeriod.add(login);
}

void LoginEnumerationProtector::setLockIfAppropriate()
{
    if (m_unauthenticatedLoginsPerPeriod.count() <= m_settings.unsuccessfulLoginsThreshold)
        return;
    if (m_unauthenticatedLoginsPerPeriod.count() <=
        m_authenticatedLoginsPerPeriod.count() * m_settings.unsuccessfulLoginsFactor)
    {
        return;
    }

    m_lockExpiration = nx::utils::monotonicTime() + m_settings.blockPeriod;
}

} // namespace nx::cdb
