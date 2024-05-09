#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nx/reflect/enum_instrument.h>

namespace nx::cloud::db::api {

struct VmsConnectionData
{
    std::string systemId;
    std::string endpoint;
};

using VmsConnectionDataList = std::vector<VmsConnectionData>;

struct Statistics
{
    int onlineServerCount = 0;
};

struct AccountLock
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(LockType,
        unknown,
        // An account locked by IP address.
        host,
        // An account locked by username.
        user
    );

    std::string key;
    LockType lockType = LockType::unknown;
};

using AccountLockList = std::vector<AccountLock>;

} // namespace nx::cloud::db::api
