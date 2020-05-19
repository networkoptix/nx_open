#include "event_rule_watcher.h"

#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <core/resource/camera_resource.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::vms::event;

EventRuleWatcher::EventRuleWatcher(RuleManager* ruleManager):
    m_ruleManager(ruleManager)
{
    connect(
        m_ruleManager, &RuleManager::rulesReset,
        this, &EventRuleWatcher::at_rulesReset);

    connect(
        m_ruleManager, &RuleManager::ruleAddedOrUpdated,
        this, &EventRuleWatcher::at_ruleAddedOrUpdated);

    connect(
        m_ruleManager, &RuleManager::ruleRemoved,
        this, &EventRuleWatcher::at_ruleRemoved);
}

EventRuleWatcher::~EventRuleWatcher()
{
}

void EventRuleWatcher::at_rulesReset(const RuleList& /*rules*/)
{
    if (recalculateWatchedEventTypes())
        emit watchedEventTypesChanged();
}

void EventRuleWatcher::at_ruleAddedOrUpdated(const RulePtr& /*rule*/, bool /*added*/)
{
    if (recalculateWatchedEventTypes())
        emit watchedEventTypesChanged();
}

void EventRuleWatcher::at_ruleRemoved(const QnUuid& /*ruleId*/)
{
    if (recalculateWatchedEventTypes())
        emit watchedEventTypesChanged();
}

std::set<QString> EventRuleWatcher::watchedEventsForDevice(
    const QnVirtualCameraResourcePtr& device) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    std::set<QString> result;

    for (const auto& deviceId: {device->getId(), QnUuid()})
    {
        const auto it = m_watchedEventTypesByDevice.find(deviceId);
        if (it == m_watchedEventTypesByDevice.cend())
            continue;

        const WatchedEventTypes& watchedEventTypes = it->second;
        result.insert(
            watchedEventTypes.analyticsEventTypes.begin(),
            watchedEventTypes.analyticsEventTypes.end());

        for (nx::vms::api::EventType eventType: watchedEventTypes.regularEventTypes)
        {
            const QString analyticsEventType = device->vmsToAnalyticsEventTypeId(eventType);
            if (!analyticsEventType.isEmpty())
                result.insert(analyticsEventType);
        }
    }

    return result;
}

bool EventRuleWatcher::recalculateWatchedEventTypes()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    std::map<QnUuid, WatchedEventTypes> watchedEventTypesByDevice;

    const RuleList rules = m_ruleManager->rules();

    for (const RulePtr& rule: rules)
    {
        if (rule->isDisabled())
            continue;

        const nx::vms::api::EventType ruleEventType = rule->eventType();
        QVector<QnUuid> ruleResourceIds = rule->eventResources();
        if (ruleResourceIds.empty())
            ruleResourceIds.push_back(QnUuid());

        for (const QnUuid& resourceId: ruleResourceIds)
        {
            WatchedEventTypes& watchedEventTypes = watchedEventTypesByDevice[resourceId];
            if (ruleEventType == nx::vms::api::EventType::analyticsSdkEvent)
            {
                watchedEventTypes.analyticsEventTypes.insert(
                    rule->eventParams().getAnalyticsEventTypeId());
            }
            else
            {
                watchedEventTypes.regularEventTypes.insert(ruleEventType);
            }
        }
    }

    if (watchedEventTypesByDevice != m_watchedEventTypesByDevice)
    {
        m_watchedEventTypesByDevice = std::move(watchedEventTypesByDevice);
        return true;
    }

    return false;
}

} // namespace analytics
} // namespace vms::server
} // namespace nx
