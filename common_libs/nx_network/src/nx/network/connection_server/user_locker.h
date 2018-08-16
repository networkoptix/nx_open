#pragma once

#include <chrono>
#include <string>
#include <tuple>

#include <nx/network/socket_common.h>
#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/thread/mutex.h>

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
    enum class AuthResult
    {
        success,
        failure,
    };

    UserLocker(const UserLockerSettings& settings);

    void updateLockoutState(AuthResult authResult);
    bool isLocked() const;

private:
    const UserLockerSettings m_settings;
    nx::utils::math::SumPerPeriod<int> m_failedAuthCountCalculator;
    std::optional<std::chrono::steady_clock::time_point> m_userLockedUntil;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API UserLockerPool
{
public:
    using Key = std::tuple<nx::network::HostAddress /*source*/, std::string /*userName*/>;

    UserLockerPool(const UserLockerSettings& settings);

    void updateLockoutState(const Key& key, UserLocker::AuthResult authResult);
    bool isLocked(const Key& key) const;

    std::map<Key, UserLocker> userLockers() const;

private:
    const UserLockerSettings m_settings;
    mutable QnMutex m_mutex;
    std::map<Key, UserLocker> m_userLockers;
    mutable utils::ElapsedTimerPool<Key> m_timers;
    const std::chrono::milliseconds m_unusedLockerExpirationPeriod;

    void removeLocker(const Key& key);
};

} // namespace server
} // namespace network
} // namespace nx
