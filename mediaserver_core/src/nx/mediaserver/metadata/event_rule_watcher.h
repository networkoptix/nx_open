#pragma once

#include <map>
#include <set>

#include <utils/common/connective.h>

#include <nx/vms/event/rule_manager.h>
#include <nx/mediaserver/metadata/rule_holder.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class EventRuleWatcher: public Connective<QObject>
{
    Q_OBJECT
public:
    EventRuleWatcher(QObject* parent);
    virtual ~EventRuleWatcher();

    void at_rulesReset(const nx::vms::event::RuleList& rules);
    void at_ruleAddedOrUpdated(const nx::vms::event::RulePtr& rule, bool added);
    void at_ruleRemoved(const QnUuid& ruleId);

    std::set<QnUuid> watchedEventsForResource(const QnUuid& resourceId);

signals:
    void rulesUpdated(const std::set<QnUuid>& affectedResources);

private:
    RuleHolder m_ruleHolder;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx

Q_DECLARE_METATYPE(std::set<QnUuid>);
