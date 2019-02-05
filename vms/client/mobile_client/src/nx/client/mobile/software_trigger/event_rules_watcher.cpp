#include "event_rules_watcher.h"

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <managers/event_rules_manager.h>

#include <nx_ec/data/api_conversion_functions.h>

namespace nx {
namespace client {
namespace mobile {

using namespace nx::vms::event;

EventRulesWatcher::EventRulesWatcher(QObject* parent):
    base_type(parent)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto rulesManager = commonModule->eventRuleManager();

    connect(rulesManager, &RuleManager::rulesReset,
        this, &EventRulesWatcher::handleRulesReset);
}

void EventRulesWatcher::handleRulesReset(const nx::vms::event::RuleList& rules)
{
    if (!rules.isEmpty() || m_updated)
        return;

    const auto commonModule = qnClientCoreModule->commonModule();
    const auto serverId = commonModule->remoteGUID();
    if (serverId.isNull())
        return;

    const auto connection = commonModule->ec2Connection();
    if (!connection)
        return;

    const auto manager = connection->makeEventRulesManager(Qn::kSystemAccess);
    nx::vms::api::EventRuleDataList receivedRules;
    if (manager->getEventRulesSync(&receivedRules) != ec2::ErrorCode::ok)
        return;

    m_updated = true;

    vms::event::RuleList ruleList;
    ec2::fromApiToResourceList(receivedRules, ruleList);
    const auto eventRuleManager = commonModule->eventRuleManager();
    eventRuleManager->resetRules(ruleList);
}

void EventRulesWatcher::handleConnectionChanged()
{
    m_updated = false;
}

} // namespace mobile
} // namespace client
} // namespace mobile
