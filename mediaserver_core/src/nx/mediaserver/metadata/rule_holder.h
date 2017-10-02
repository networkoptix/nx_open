#pragma once

#include <set>
#include <map>

#include <nx/vms/event/event_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <common/common_module_aware.h>

namespace nx {
namespace mediaserver {
namespace metadata {

struct ResourceEvents
{
    ResourceEvents(const QnUuid& resId, const std::set<QnUuid>& resEvents):
        resourceId(resId),
        resourceEvents(resEvents)
    {
    }

    QnUuid resourceId;
    std::set<QnUuid> resourceEvents;
};

class RuleHolder: public QnCommonModuleAware
{
    using RuleId = QnUuid;
    using ResourceId = QnUuid;
    using EventTypeId = QnUuid;
    using AffectedResources = std::set<QnUuid>;
public:

    RuleHolder(QnCommonModule* commonModule);
    AffectedResources addRule(const nx::vms::event::RulePtr& rule);
    AffectedResources updateRule(const nx::vms::event::RulePtr& rule);
    AffectedResources removeRule(const QnUuid& ruleId);
    AffectedResources resetRules(const nx::vms::event::RuleList& rules);

    std::set<QnUuid> watchedEvents(const QnUuid& resourceId) const;

private:
    AffectedResources addRuleUnsafe(const nx::vms::event::RulePtr& rule);
    AffectedResources removeRuleUnsafe(const QnUuid& ruleId);

    std::set<QnUuid> eventResources(const QVector<QnUuid>& resources) const;
    bool isRuleBeingWatched(const QnUuid& ruleId) const;
    bool isAnalyticsSdkEventRule(const nx::vms::event::RulePtr& ruleId) const;
    QnUuid analyticsEventIdFromRule(const nx::vms::event::RulePtr& rule) const;
    std::set<QnUuid> calculateResourceEvents(const QnUuid& resourceId) const;

private:
    mutable QnMutex m_mutex;
    std::map<RuleId, std::set<ResourceId>> m_ruleResourceMap;
    std::map<ResourceId, std::set<RuleId>> m_resourceRuleMap;
    std::map<ResourceId, std::set<EventTypeId>> m_resourceEventMap;
    std::map<RuleId, EventTypeId> m_ruleEventMap;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx