#ifndef QN_BUSINESS_EVENT_RULE_H
#define QN_BUSINESS_EVENT_RULE_H

#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/business_fwd.h>

#include <core/resource/resource_fwd.h>
#include "business_action_parameters.h"

/**
* This class define relation between business event and action
*/
class QnBusinessEventRule: public QObject
{
    Q_OBJECT

public:
    QnBusinessEventRule();
    ~QnBusinessEventRule();

    QString getUniqueId() const;

    QnId id() const;
    void setId(const QnId& value);

    BusinessEventType::Value eventType() const;
    void setEventType(const BusinessEventType::Value value);

    QnResourceList eventResources() const;
    void setEventResources(const QnResourceList &value);

    QnBusinessEventParameters eventParams() const;
    void setEventParams(const QnBusinessEventParameters& params);

    Qn::ToggleState eventState() const;
    void setEventState(Qn::ToggleState state);

    BusinessActionType::Value actionType() const;
    void setActionType(const BusinessActionType::Value value);

    QnResourceList actionResources() const;
    void setActionResources(const QnResourceList &value);

    QnBusinessActionParameters actionParams() const;
    void setActionParams(const QnBusinessActionParameters& params);

    /* action aggregation period in seconds */
    int aggregationPeriod() const;
    void setAggregationPeriod(int secs);

    bool disabled() const;
    void setDisabled(bool value);

    QString schedule() const;
    void setSchedule(const QString value);

    QString comments() const;
    void setComments(const QString value);

    bool system() const;
    void setSystem(bool value);

    /* Check if current time allowed in schedule */
    bool isScheduleMatchTime(const QDateTime& datetime) const;

    static QnBusinessEventRuleList getDefaultRules();

    QnBusinessEventRule* clone();
    void removeResource(const QnId& resId);
private:
    QnBusinessEventRule(int internalId, int aggregationPeriod, const QByteArray& actionParams, bool isSystem, BusinessActionType::Value bActionType, BusinessEventType::Value bEventType, QnResourcePtr actionRes= QnResourcePtr());

    QnId m_id;

    BusinessEventType::Value m_eventType;
    QnResourceList m_eventResources;
    QnBusinessEventParameters m_eventParams;
    Qn::ToggleState m_eventState;

    BusinessActionType::Value m_actionType;
    QnResourceList m_actionResources;
    QnBusinessActionParameters m_actionParams;

    int m_aggregationPeriod;
    bool m_disabled;

    QString m_schedule;
    QByteArray m_binSchedule;
    QString m_comments;

    bool m_system;
};

Q_DECLARE_METATYPE(QnBusinessEventRulePtr)
Q_DECLARE_METATYPE(QnBusinessEventRuleList)

#endif // QN_BUSINESS_EVENT_RULE_H
