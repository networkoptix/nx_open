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

    QUuid id() const;
    void setId(const QUuid& value);

    QnBusiness::EventType eventType() const;
    void setEventType(QnBusiness::EventType eventType);

    QVector<QUuid> eventResources() const;
    void setEventResources(const QVector<QUuid> &value);
    QnResourceList eventResourceObjects() const;

    QnBusinessEventParameters eventParams() const;
    void setEventParams(const QnBusinessEventParameters& params);

    QnBusiness::EventState eventState() const;
    void setEventState(QnBusiness::EventState state);

    QnBusiness::ActionType actionType() const;
    void setActionType(QnBusiness::ActionType actionType);

    QVector<QUuid> actionResources() const;
    QnResourceList actionResourceObjects() const;
    void setActionResources(const QVector<QUuid> &value);

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

    QnBusinessEventRule* clone();
    void removeResource(const QUuid& resId);

private:
    QnBusinessEventRule(int internalId, int aggregationPeriod, const QByteArray& actionParams, bool isSystem,
        QnBusiness::ActionType bActionType, QnBusiness::EventType bEventType, const QnResourcePtr& actionRes= QnResourcePtr());

    QUuid m_id;

    QnBusiness::EventType m_eventType;
    QVector<QUuid> m_eventResources;
    QnBusinessEventParameters m_eventParams;
    QnBusiness::EventState m_eventState;

    QnBusiness::ActionType m_actionType;
    QVector<QUuid> m_actionResources;
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
