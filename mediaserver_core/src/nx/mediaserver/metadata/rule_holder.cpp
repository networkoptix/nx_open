#include "rule_holder.h"

#include <vector>
#include <algorithm>

#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/mediaserver/metadata/event_type_mapping.h>

#include <common/common_module.h>
#include <plugins/plugin_internal_tools.h>

#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::vms;

RuleHolder::RuleHolder(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

RuleHolder::AffectedResources RuleHolder::addRule(const nx::vms::event::RulePtr& rule)
{
    QnMutexLocker lock(&m_mutex);
    return addRuleUnsafe(rule);
}

RuleHolder::AffectedResources RuleHolder::updateRule(const nx::vms::event::RulePtr& rule)
{
    QnMutexLocker lock(&m_mutex);
    const bool ruleIsNeededToBeWatched = needToWatchRule(rule);
    const bool ruleIsBeingWatched = isRuleBeingWatched(rule->id());

    if (!ruleIsBeingWatched && !ruleIsNeededToBeWatched)
        return AffectedResources();

    if (ruleIsBeingWatched && !ruleIsNeededToBeWatched)
        return removeRuleUnsafe(rule->id());

    if (!ruleIsBeingWatched && ruleIsNeededToBeWatched)
        return addRuleUnsafe(rule);

    if (ruleIsBeingWatched && ruleIsNeededToBeWatched)
        return updateRuleUnsafe(rule);

    return handleChanges();
}

RuleHolder::AffectedResources RuleHolder::removeRule(const QnUuid& ruleId)
{
    QnMutexLocker lock(&m_mutex);
    return removeRuleUnsafe(ruleId);
}

RuleHolder::AffectedResources RuleHolder::resetRules(const nx::vms::event::RuleList& rules)
{
    QnMutexLocker lock(&m_mutex);
    m_watchedRules.clear();
    m_anyResourceRules.clear();

    for (const auto& rule: rules)
    {
        if (isAnyResourceRule(rule))
            m_anyResourceRules.insert(rule->id());
        else
            m_watchedRules.insert(rule->id());
    }

    return handleChanges();
}

RuleHolder::AffectedResources RuleHolder::addResource(const QnResourcePtr& resourcePtr)
{
    QnMutexLocker lock(&m_mutex);
    return handleChanges();
}

RuleHolder::EventIds RuleHolder::watchedEvents(const QnUuid& resourceId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_watchedEvents.value(resourceId);
}

RuleHolder::AffectedResources RuleHolder::addRuleUnsafe(const nx::vms::event::RulePtr& rule)
{
    if (!needToWatchRule(rule))
        return QSet<QnUuid>();

    if (isAnyResourceRule(rule))
        m_anyResourceRules.insert(rule->id());
    else
        m_watchedRules.insert(rule->id());

    return handleChanges();
}

RuleHolder::AffectedResources RuleHolder::removeRuleUnsafe(const QnUuid& ruleId)
{
    if (!isRuleBeingWatched(ruleId))
        return RuleHolder::AffectedResources();

    m_watchedRules.remove(ruleId);
    m_anyResourceRules.remove(ruleId);

    return handleChanges();
}

RuleHolder::AffectedResources RuleHolder::updateRuleUnsafe(const nx::vms::event::RulePtr& rule)
{
    if (isAnyResourceRule(rule))
    {
        m_watchedRules.remove(rule->id());
        m_anyResourceRules.insert(rule->id());
    }
    else
    {
        m_anyResourceRules.remove(rule->id());
        m_watchedRules.insert(rule->id());
    }

    return handleChanges();
}

bool RuleHolder::isAnyResourceRule(const nx::vms::event::RulePtr& rule) const
{
    return rule && rule->eventResources().isEmpty();
}

bool RuleHolder::isRuleBeingWatched(const QnUuid& ruleId) const
{
    return m_watchedRules.contains(ruleId)
        || m_anyResourceRules.contains(ruleId);
}

bool RuleHolder::needToWatchRule(const event::RulePtr& rule) const
{
    if (isAnyResourceRule(rule))
        return !rule->isDisabled();

    const auto ruleEventType = rule->eventType();
    const auto ruleResources = rule->eventResources();

    const bool isAnalyticsRule = std::any_of(
        ruleResources.cbegin(),
        ruleResources.cend(),
        [ruleEventType, this](const QnUuid& resourceId)
        {
            const auto resourcePool = commonModule()->resourcePool();
            const auto camera = resourcePool->getResourceById<QnSecurityCamResource>(resourceId);
            if (!camera)
                return false;

            return camera->doesEventComeFromAnalyticsDriver(ruleEventType);
        });

    return isAnalyticsRule && !rule->isDisabled();
}

QnUuid RuleHolder::analyticsEventIdFromRule(const nx::vms::event::RulePtr& rule) const
{
    if (rule->eventType() == nx::vms::event::EventType::analyticsSdkEvent)
        return rule->eventParams().analyticsEventId();

    return guidByEventType(rule->eventType());
}

RuleHolder::ResourceEvents RuleHolder::calculateWatchedEvents() const
{
    ResourceEvents events;
    const auto ruleManager = commonModule()
        ->eventRuleManager();

    const auto resourcePool = commonModule()
        ->resourcePool();

    for (const auto& ruleId: m_watchedRules)
    {
        const auto rule = ruleManager->rule(ruleId);
        if (!rule)
            continue;

        const auto eventId = analyticsEventIdFromRule(rule);
        const auto resourceIds = rule->eventResources();

        for (const auto& resourceId: resourceIds)
           events[resourceId].insert(eventId);
    }

    for (const auto& ruleId: m_anyResourceRules)
    {
        const auto rule = ruleManager->rule(ruleId);
        if (!rule)
            continue;

        const auto eventId = analyticsEventIdFromRule(rule);
        if (eventId.isNull())
            continue;

        const auto eventType = rule->eventType();
        for (const auto& camera: resourcePool->getResources<QnSecurityCamResource>())
        {
            if (camera->doesEventComeFromAnalyticsDriver(eventType))
                events[camera->getId()].insert(eventId);
        }
    }

    return events;
}

RuleHolder::AffectedResources RuleHolder::handleChanges()
{
    AffectedResources affectedResources;
    auto newWatchedEvents = calculateWatchedEvents();

    for (const auto& resourceId: newWatchedEvents.keys() + m_watchedEvents.keys())
    {
        if (newWatchedEvents[resourceId] != m_watchedEvents[resourceId])
            affectedResources.insert(resourceId);
    }

    m_watchedEvents.swap(newWatchedEvents);

    return affectedResources;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
