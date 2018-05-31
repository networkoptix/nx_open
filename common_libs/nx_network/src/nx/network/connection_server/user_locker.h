#pragma once

#include <chrono>
#include <string>
#include <tuple>

#include <nx/network/socket_common.h>
#include <nx/utils/math/sum_per_period.h>

namespace nx {
namespace network {
namespace server {

struct UserLockerSettings
{
    std::chrono::milliseconds checkPeriod;
    int authFailureCount = 0;
    std::chrono::milliseconds lockPeriod;
};

/**
 * If there were not less than authFailureCount login failures during last checkPeriod,
 * then username is locked for another lockPeriod.
 */
class NX_NETWORK_API UserLocker
{
public:
    enum class AuthResult
    {
        success,
        failure,
    };

    using Key = std::tuple<nx::network::HostAddress /*source*/, std::string /*userName*/>;

    UserLocker(const UserLockerSettings& settings);

    void updateLockoutState(const Key& key, AuthResult authResult);
    bool isLocked(const Key& key) const;

private:
    const UserLockerSettings m_settings;
    nx::utils::math::SumPerPeriod<int> m_failedAuthCountCalculator;
    std::optional<std::chrono::steady_clock::time_point> m_userLockedUntil;
};

} // namespace server
} // namespace network
} // namespace nx
