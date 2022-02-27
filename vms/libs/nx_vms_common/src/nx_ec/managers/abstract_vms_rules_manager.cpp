// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_vms_rules_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractVmsRulesManager::getVmsRulesSync(nx::vms::api::rules::RuleList* outRuleList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getVmsRules(std::move(handler));
        },
        outRuleList);
}

ErrorCode AbstractVmsRulesManager::saveSync(const nx::vms::api::rules::Rule& rule)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(rule, std::move(handler));
        });
}

ErrorCode AbstractVmsRulesManager::deleteRuleSync(const QnUuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            deleteRule(id, std::move(handler));
        });
}

ErrorCode AbstractVmsRulesManager::broadcastEventSync(const nx::vms::api::rules::EventInfo& info)
{
    return detail::callSync(
        [&](auto handler)
        {
            broadcastEvent(info, std::move(handler));
        });
}

ErrorCode AbstractVmsRulesManager::resetVmsRulesSync()
{
    return detail::callSync(
        [&](auto handler)
        {
            resetVmsRules(std::move(handler));
        });
}

} // namespace ec2
