#pragma once

#include <chrono>
#include <string>
#include <tuple>

#include <nx/network/socket_common.h>
#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/thread/mutex.h>

#include "access_blocker_pool.h"

namespace nx {
namespace network {
namespace server {

struct UserLockerSettings
{
    std::chrono::milliseconds checkPeriod = std::chrono::minutes(5);
    int authFailureCount = 10;
    std::chrono::milliseconds lockPeriod = std::chrono::minutes(1);
};

/**
 * If there were not less than authFailureCount login failures during last checkPeriod,
 * then username is locked for another lockPeriod.
 */
class NX_NETWORK_API UserLocker
{
public:
    UserLocker(const UserLockerSettings& settings);

    LockUpdateResult updateLockoutState(AuthResult authResult);
    bool isLocked() const;

private:
    const UserLockerSettings m_settings;
    nx::utils::math::SumPerPeriod<int> m_failedAuthCountCalculator;
    std::optional<std::chrono::steady_clock::time_point> m_userLockedUntil;
};

//-------------------------------------------------------------------------------------------------

template<typename UpdateHistoryParams>
using UserAccessBlockerPool = AccessBlockerPool<
    std::tuple<nx::network::HostAddress /*source*/, std::string /*userName*/>,
    UserLocker,
    UserLockerSettings,
    UpdateHistoryParams>;

template<typename UpdateHistoryParams = void>
class UserLockerPool:
    public UserAccessBlockerPool<UpdateHistoryParams>
{
    using base_type = UserAccessBlockerPool<UpdateHistoryParams>;

public:
    UserLockerPool(const UserLockerSettings& settings):
        base_type(
            settings,
            std::max(settings.checkPeriod, settings.lockPeriod))
    {
    }
};

} // namespace server
} // namespace network
} // namespace nx
