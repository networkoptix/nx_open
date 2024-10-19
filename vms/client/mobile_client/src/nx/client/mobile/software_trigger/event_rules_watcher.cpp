// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_rules_watcher.h"

#include <managers/event_rules_manager.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx {
namespace client {
namespace mobile {

using namespace nx::vms::event;

EventRulesWatcher::EventRulesWatcher(QObject* parent):
    base_type(parent)
{
    const auto rulesManager = systemContext()->eventRuleManager();

    connect(rulesManager, &RuleManager::rulesReset,
        this, &EventRulesWatcher::handleRulesReset);
}

void EventRulesWatcher::handleRulesReset(const nx::vms::event::RuleList& rules)
{
    if (!rules.isEmpty() || m_updated)
        return;

    const auto serverId = systemContext()->currentServerId();
    if (serverId.isNull())
        return;

    const auto connection = systemContext()->messageBusConnection();
    if (!connection)
        return;

    const auto manager = connection->getEventRulesManager(nx::network::rest::kSystemSession);
    nx::vms::api::EventRuleDataList receivedRules;
    if (manager->getEventRulesSync(&receivedRules) != ec2::ErrorCode::ok)
        return;

    m_updated = true;

    vms::event::RuleList ruleList;
    ec2::fromApiToResourceList(receivedRules, ruleList);
    const auto eventRuleManager = systemContext()->eventRuleManager();
    eventRuleManager->resetRules(ruleList);
}

void EventRulesWatcher::handleConnectionChanged()
{
    m_updated = false;
}

} // namespace mobile
} // namespace client
} // namespace mobile
