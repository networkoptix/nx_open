#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <utils/common/id.h>
#include <core/resource/resource_fwd.h>

#include <business/business_fwd.h>
#include <business/business_action_parameters.h>
#include <business/business_event_parameters.h>

namespace QnBusiness
{
    //TODO: #GDM #Business fix to resourceTypeRequired: None, Camera, Server, User, etc
    bool requiresCameraResource(ActionType actionType);
    bool requiresUserResource(ActionType actionType);

    bool hasToggleState(ActionType actionType);

    QList<ActionType> allActions();
}


/**
 * Base class for business actions
 */
class QnAbstractBusinessAction
{
protected:

    explicit QnAbstractBusinessAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters& runtimeParams);

public:
    virtual ~QnAbstractBusinessAction();
    QnBusiness::ActionType actionType() const { return m_actionType; }

    /**
     * Resource depend of action type.
     * see: QnBusiness::requiresCameraResource()
     * see: QnBusiness::requiresUserResource()
     */
    void setResources(const QVector<QnUuid>& resources);

    const QVector<QnUuid>& getResources() const;
    QnResourceList getResourceObjects() const;

    void setParams(const QnBusinessActionParameters& params);
    const QnBusinessActionParameters& getParams() const;
    QnBusinessActionParameters& getParams();

    void setRuntimeParams(const QnBusinessEventParameters& params);
    const QnBusinessEventParameters& getRuntimeParams() const;

    void setBusinessRuleId(const QnUuid& value);
    QnUuid getBusinessRuleId() const;

    void setToggleState(QnBusiness::EventState value);
    QnBusiness::EventState getToggleState() const;

    void setReceivedFromRemoteHost(bool value);
    bool isReceivedFromRemoteHost() const;

    /** How much times action was instantiated during the aggregation period. */
    void setAggregationCount(int value);
    int getAggregationCount() const;

    /** Return action unique key for external outfit (port number for output action e.t.c). Do not count resourceId 
     * This function help to share physical resources between actions
     * Do not used for instant actions
     */
    virtual QString getExternalUniqKey() const;

protected:
    QnBusiness::ActionType m_actionType;
    QnBusiness::EventState m_toggleState;
    bool m_receivedFromRemoteHost;
    QVector<QnUuid> m_resources;
    QnBusinessActionParameters m_params;
    QnBusinessEventParameters m_runtimeParams;
    QnUuid m_businessRuleId; // business rule that generated this action
    int m_aggregationCount;
};


class QnBusinessActionData
{
public:
    // TODO: #EC2 Add comments. Maybe remove the flag altogether. What is it for? Which actions?
    enum Flags {
        MotionExists = 1
    };

    QnBusinessActionData(): m_actionType(QnBusiness::UndefinedAction), m_flags(0) {}

    QnBusiness::ActionType actionType() const { return m_actionType; }
    void setActionType(QnBusiness::ActionType type) { m_actionType = type; }

    void setParams(const QnBusinessActionParameters& params) { m_params = params;}
    const QnBusinessActionParameters& getParams() const { return m_params; }

    void setRuntimeParams(const QnBusinessEventParameters& params) {m_runtimeParams = params;}
    const QnBusinessEventParameters& getRuntimeParams() const {return m_runtimeParams; }

    void setBusinessRuleId(const QnUuid& value) {m_businessRuleId = value; }
    QnUuid getBusinessRuleId() const { return m_businessRuleId; }

    void setAggregationCount(int value) { m_aggregationCount = value; }
    int getAggregationCount() const { return m_aggregationCount; }

    qint64 timestamp() const { return m_runtimeParams.eventTimestamp; }
    QnBusiness::EventType eventType() const { return m_runtimeParams.eventType; }

    void setCompareString(const QString& value) { m_compareString = value; }
    const QString& compareString() const { return m_compareString; }

    void setFlags(int value) { m_flags = value; }
    int flags() const { return m_flags; }
    bool hasFlags(int flags) const { return m_flags & flags; }

protected:
    QnBusiness::ActionType m_actionType;
    QnBusinessActionParameters m_params;
    QnBusinessEventParameters m_runtimeParams;
    QnUuid m_businessRuleId; 
    int m_aggregationCount;
    QString m_compareString;
    int m_flags;
};

Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)
Q_DECLARE_METATYPE(QnAbstractBusinessActionList)
Q_DECLARE_METATYPE(QnBusinessActionDataList)
Q_DECLARE_METATYPE(QnBusinessActionDataListPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
