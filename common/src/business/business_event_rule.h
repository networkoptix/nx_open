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

    QnUuid id() const;
    void setId(const QnUuid& value);

    QnBusiness::EventType eventType() const;
    void setEventType(QnBusiness::EventType eventType);

    QVector<QnUuid> eventResources() const;
    void setEventResources(const QVector<QnUuid> &value);
    QnResourceList eventResourceObjects() const;

    QnBusinessEventParameters eventParams() const;
    void setEventParams(const QnBusinessEventParameters& params);

    QnBusiness::EventState eventState() const;
    void setEventState(QnBusiness::EventState state);

    QnBusiness::ActionType actionType() const;
    void setActionType(QnBusiness::ActionType actionType);

    QVector<QnUuid> actionResources() const;
    QnResourceList actionResourceObjects() const;
    void setActionResources(const QVector<QnUuid> &value);

    QnBusinessActionParameters actionParams() const;
    void setActionParams(const QnBusinessActionParameters& params);

    /* action aggregation period in seconds */
    int aggregationPeriod() const;
    void setAggregationPeriod(int secs);

    bool isDisabled() const;
    void setDisabled(bool disabled);

    QString schedule() const;
    void setSchedule(const QString &schedule);

    QString comment() const;
    void setComment(const QString &comment);

    bool isSystem() const;
    void setSystem(bool system);

    /* Check if current time allowed in schedule */
    bool isScheduleMatchTime(const QDateTime& datetime) const;

    static QnBusinessEventRuleList getDefaultRules();
    static QnBusinessEventRuleList getSystemRules();

    QnBusinessEventRule* clone();
    void removeResource(const QnUuid& resId);

private:
    QnBusinessEventRule(int internalId, int aggregationPeriod, const QByteArray& actionParams, bool isSystem,
        QnBusiness::ActionType bActionType, QnBusiness::EventType bEventType, const QnResourcePtr& actionRes= QnResourcePtr());

    QnUuid m_id;

    QnBusiness::EventType m_eventType;
    QVector<QnUuid> m_eventResources;
    QnBusinessEventParameters m_eventParams;
    QnBusiness::EventState m_eventState;

    QnBusiness::ActionType m_actionType;
    QVector<QnUuid> m_actionResources;
    QnBusinessActionParameters m_actionParams;

    int m_aggregationPeriod;
    bool m_disabled;

    QString m_schedule;
    QByteArray m_binSchedule;
    QString m_comment;

    bool m_system;
};

Q_DECLARE_METATYPE(QnBusinessEventRulePtr)
Q_DECLARE_METATYPE(QnBusinessEventRuleList)

#endif // QN_BUSINESS_EVENT_RULE_H
