// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "id_cache.h"

#include <nx/sdk/helpers/uuid_helper.h>

//#include <nx/utils/uuid.h>

namespace nx::vms_server_plugins::analytics::bosch {

bool IdCache::alreadyContains(int id)
{
    using namespace std::chrono;

    for (auto it = m_map.begin(); it != m_map.end();)
    {
        if (it->first == id)
        {
            it->second.restart();
            return true;
        }
        else
        {
            if (it->second.hasExpired(m_timeout))
                it = m_map.erase(it);
            else
                ++it;
        }
    }
    m_map.emplace(id, nx::utils::ElapsedTimer(true /*started*/));
    return false;
}

nx::sdk::Uuid UuidCache::makeUuid(int id)
{
    using namespace std::chrono;

    for (auto it = m_map.begin(); it != m_map.end();)
    {
        if (it->first == id)
        {
            it->second.timer.restart();
            return it->second.uuid;
        }
        else
        {
            if (it->second.timer.hasExpired(m_timeout))
                it = m_map.erase(it);
            else
                ++it;
        }
    }

    const auto uuid = nx::sdk::UuidHelper::randomUuid();
    m_map.emplace(id, UuidData{uuid, nx::utils::ElapsedTimer(true /*started*/)});
    return uuid;
}

} // namespace nx::vms_server_plugins::analytics::bosch
