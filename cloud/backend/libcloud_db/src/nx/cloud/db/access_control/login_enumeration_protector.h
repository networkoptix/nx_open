#pragma once

#include <chrono>

#include <nx/network/connection_server/access_blocker_pool.h>
#include <nx/utils/progressive_delay_calculator.h>
#include <nx/utils/math/unique_value_count_per_period.h>
#include <nx/utils/std/optional.h>

namespace nx::cloud::db {

struct LoginEnumerationProtectionSettings
{
    std::chrono::seconds period = std::chrono::minutes(1);
    int unsuccessfulLoginsThreshold = 10;
    double unsuccessfulLoginsFactor = 0.3;
    std::chrono::seconds minBlockPeriod = std::chrono::minutes(1);
    std::chrono::seconds maxBlockPeriod = std::chrono::minutes(30);
};

class LoginEnumerationProtector
{
public:
    LoginEnumerationProtector(
        const LoginEnumerationProtectionSettings& settings);

    /**
     * @param lockPeriod Set to lock period if lock is set.
     */
    nx::network::server::LockUpdateResult updateLockoutState(
        nx::network::server::AuthResult authResult,
        const std::string& login,
        std::chrono::milliseconds* lockPeriod);

    bool isLocked() const;

private:
    const LoginEnumerationProtectionSettings m_settings;
    nx::utils::math::UniqueValueCountPerPeriod<std::string> m_authenticatedLoginsPerPeriod;
    nx::utils::math::UniqueValueCountPerPeriod<std::string> m_unauthenticatedLoginsPerPeriod;
    std::optional<std::chrono::steady_clock::time_point> m_lockExpiration;
    nx::utils::ProgressiveDelayCalculator m_delayCalculator;

    bool removeExpiredLock();

    void updateCounters(
        nx::network::server::AuthResult authResult,
        const std::string& login);

    void setLockIfAppropriate(std::chrono::milliseconds* lockPeriod);
};

} // namespace nx::cloud::db
