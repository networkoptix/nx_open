// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>
#include <tuple>

#include <nx/network/socket_common.h>
#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/thread/mutex.h>

#include "access_blocker_pool.h"

class SettingsReader;

namespace nx {
namespace network {
namespace server {

struct NX_NETWORK_API UserLockerSettings
{
    std::chrono::milliseconds checkPeriod = std::chrono::minutes(5);
    int authFailureCount = 10;
    int maxLockerCount = 10'000;
    std::chrono::milliseconds lockPeriod = std::chrono::minutes(1);

    void load(const SettingsReader&);
};

NX_REFLECTION_INSTRUMENT(UserLockerSettings, (checkPeriod)(authFailureCount)(maxLockerCount)(lockPeriod))

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

struct UserKey
{
    std::string hostAddress;
    std::string username;

    bool operator<(const UserKey& other) const
    {
        return hostAddress < other.hostAddress && username < other.username;
    }
};

template<typename UpdateHistoryParams>
using UserAccessBlockerPool = AccessBlockerPool<
    UserKey,
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
            std::max(settings.checkPeriod, settings.lockPeriod),
            settings.maxLockerCount)
    {
    }
};

} // namespace server
} // namespace network
} // namespace nx
