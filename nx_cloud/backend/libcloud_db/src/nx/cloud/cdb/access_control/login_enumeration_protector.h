#pragma once

#include <chrono>

#include <nx/network/connection_server/access_blocker_pool.h>
#include <nx/utils/math/unique_value_count_per_period.h>
#include <nx/utils/std/optional.h>

namespace nx::cdb {

struct LoginEnumerationProtectionSettings
{
    std::chrono::seconds period = std::chrono::minutes(1);
    int unsuccessfulLoginsThreshold = 10;
    double unsuccessfulLoginsFactor = 0.3;
    std::chrono::seconds blockPeriod = std::chrono::minutes(1);
};

class LoginEnumerationProtector
{
public:
    LoginEnumerationProtector(
        const LoginEnumerationProtectionSettings& settings);

    nx::network::server::LockUpdateResult updateLockoutState(
        nx::network::server::AuthResult authResult,
        const std::string& login);

    bool isLocked() const;

private:
    const LoginEnumerationProtectionSettings m_settings;
    nx::utils::math::UniqueValueCountPerPeriod<std::string> m_authenticatedLoginsPerPeriod;
    nx::utils::math::UniqueValueCountPerPeriod<std::string> m_unauthenticatedLoginsPerPeriod;
    std::optional<std::chrono::steady_clock::time_point> m_lockExpiration;

    bool removeExpiredLock();

    void updateCounters(
        nx::network::server::AuthResult authResult,
        const std::string& login);

    void setLockIfAppropriate();
};

} // namespace nx::cdb
