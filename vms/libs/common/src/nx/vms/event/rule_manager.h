#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <nx/vms/event/event_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace event {

class RuleManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    RuleManager(QObject* parent = nullptr);
    virtual ~RuleManager();

 // Returns current list of event rules in no particular order.
    RuleList rules() const;

 // Returns event rule by id.
    RulePtr rule(const QnUuid& id) const;

 // These methods are called by common message processor.
    void resetRules(const RuleList& rules);
    void addOrUpdateRule(const RulePtr& rule);
    void removeRule(const QnUuid& id);

signals:
    void rulesReset(const nx::vms::event::RuleList& rules);
    void ruleAddedOrUpdated(const nx::vms::event::RulePtr& rule, bool added);
    void ruleRemoved(const QnUuid& id);

private:
    mutable QnMutex m_mutex;
    QHash<QnUuid, RulePtr> m_rules;
};

} // namespace event
} // namespace vms
} // namespace nx
