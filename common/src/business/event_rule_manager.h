#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <business/business_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QnEventRuleManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnEventRuleManager(QObject* parent = nullptr);
    virtual ~QnEventRuleManager();

 // Returns current list of event rules in no particular order.
    QnBusinessEventRuleList rules() const;

 // Returns event rule by id.
    QnBusinessEventRulePtr rule(const QnUuid& id) const;

 // These methods are called by common message processor.
    void resetRules(const QnBusinessEventRuleList& rules);
    void addOrUpdateRule(const QnBusinessEventRulePtr& rule);
    void removeRule(const QnUuid& id);

signals:
    void rulesReset(const QnBusinessEventRuleList& rules);
    void ruleAddedOrUpdated(const QnBusinessEventRulePtr& rule, bool added);
    void ruleRemoved(const QnUuid& id);

private:
    mutable QnMutex m_mutex;
    QHash<QnUuid, QnBusinessEventRulePtr> m_rules;
};
