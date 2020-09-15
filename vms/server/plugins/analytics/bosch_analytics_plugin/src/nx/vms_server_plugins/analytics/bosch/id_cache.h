// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <chrono>

#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/uuid.h>

namespace nx::vms_server_plugins::analytics::bosch {

class IdCache
{
public:
    IdCache(std::chrono::milliseconds timeout = kTimeout): m_timeout(timeout) {}
    bool alreadyContains(int id);

private:
    std::map<int, nx::utils::ElapsedTimer> m_map;
    std::chrono::milliseconds m_timeout;
    static inline const std::chrono::milliseconds kTimeout{10'000};
};

struct UuidData
{
    nx::sdk::Uuid uuid;
    nx::utils::ElapsedTimer timer;
};

class UuidCache
{
public:
    UuidCache(std::chrono::milliseconds timeout = kTimeout) : m_timeout(timeout) {}
    nx::sdk::Uuid makeUuid(int id);

private:
    std::map<int, UuidData> m_map;
    std::chrono::milliseconds m_timeout;
    static inline const std::chrono::milliseconds kTimeout{10'000};
};

} // namespace nx::vms_server_plugins::analytics::bosch
