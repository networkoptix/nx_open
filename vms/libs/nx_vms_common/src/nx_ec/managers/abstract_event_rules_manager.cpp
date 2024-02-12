// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_event_rules_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractEventRulesManager::getEventRulesSync(
    nx::vms::api::EventRuleDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getEventRules(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractEventRulesManager::saveSync(const nx::vms::api::EventRuleData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractEventRulesManager::deleteRuleSync(const nx::Uuid& ruleId)
{
    return detail::callSync(
        [&](auto handler)
        {
            deleteRule(ruleId, std::move(handler));
        });
}

ErrorCode AbstractEventRulesManager::broadcastEventActionSync(
    const nx::vms::api::EventActionData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            broadcastEventAction(data, std::move(handler));
        });
}

ErrorCode AbstractEventRulesManager::resetBusinessRulesSync()
{
    return detail::callSync(
        [&](auto handler)
        {
            resetBusinessRules(std::move(handler));
        });
}

} // namespace ec2
