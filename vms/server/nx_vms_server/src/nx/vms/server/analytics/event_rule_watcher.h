#pragma once

#include <map>
#include <set>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::event { class RuleManager; } // namespace nx::vms::event

namespace nx::vms::server::analytics {

class EventRuleWatcher: public Connective<QObject>
{
    Q_OBJECT
public:
    EventRuleWatcher(nx::vms::event::RuleManager* ruleManager);
    virtual ~EventRuleWatcher();

    std::set<QString> watchedEventsForDevice(const QnVirtualCameraResourcePtr& device) const;

signals:
    void watchedEventTypesChanged();

private:
    bool recalculateWatchedEventTypes();

    void at_rulesReset(const nx::vms::event::RuleList& rules);
    void at_ruleAddedOrUpdated(const nx::vms::event::RulePtr& rule, bool added);
    void at_ruleRemoved(const QnUuid& ruleId);

private:
    struct WatchedEventTypes
    {
        std::set<QString> analyticsEventTypes;
        std::set<nx::vms::api::EventType> regularEventTypes;

        bool operator==(const WatchedEventTypes& other) const
        {
            return analyticsEventTypes == other.analyticsEventTypes
                && regularEventTypes == other.regularEventTypes;
        }
    };

    mutable nx::Mutex m_mutex;

    nx::vms::event::RuleManager* m_ruleManager = nullptr;
    std::map<QnUuid, WatchedEventTypes> m_watchedEventTypesByDevice;
};

} // namespace nx::vms::server::analytics
