// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_analytics_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractAnalyticsManager::getAnalyticsPluginsSync(
    nx::vms::api::AnalyticsPluginDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getAnalyticsPlugins(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractAnalyticsManager::getAnalyticsEnginesSync(
    nx::vms::api::AnalyticsEngineDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getAnalyticsEngines(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractAnalyticsManager::saveSync(const nx::vms::api::AnalyticsPluginData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractAnalyticsManager::saveSync(const nx::vms::api::AnalyticsEngineData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractAnalyticsManager::removeAnalyticsPluginSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeAnalyticsPlugin(id, std::move(handler));
        });
}

ErrorCode AbstractAnalyticsManager::removeAnalyticsEngineSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeAnalyticsEngine(id, std::move(handler));
        });
}

} // namespace ec2
