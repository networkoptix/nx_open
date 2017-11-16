#pragma once

#include <QtCore/QSet>
#include <QtCore/QMap>

#include <nx/vms/event/event_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <common/common_module_aware.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class RuleHolder: public QnCommonModuleAware
{
    using RuleId = QnUuid;
    using ResourceId = QnUuid;
    using EventTypeId = QnUuid;
    using AffectedResources = QSet<QnUuid>;
    using EventIds = QSet<QnUuid>;
    using ResourceEvents = QMap<ResourceId, EventIds>;

public:
    RuleHolder(QnCommonModule* commonModule);
    AffectedResources addRule(const nx::vms::event::RulePtr& rule);
    AffectedResources updateRule(const nx::vms::event::RulePtr& rule);
    AffectedResources removeRule(const QnUuid& ruleId);
    AffectedResources resetRules(const nx::vms::event::RuleList& rules);

    EventIds watchedEvents(const QnUuid& resourceId) const;

private:
    AffectedResources addRuleUnsafe(const nx::vms::event::RulePtr& rule);
    AffectedResources removeRuleUnsafe(const QnUuid& ruleId);

    bool isRuleBeingWatched(const QnUuid& ruleId) const;
    bool needToWatchRule(const nx::vms::event::RulePtr& ruleId) const;
    QnUuid analyticsEventIdFromRule(const nx::vms::event::RulePtr& rule) const;
    ResourceEvents calculateWatchedEvents() const;
    AffectedResources handleChanges();
    
private:
    mutable QnMutex m_mutex;

    QSet<RuleId> m_watchedRules;
    ResourceEvents m_watchedEvents;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx