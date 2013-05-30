#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QtCore/QSharedPointer>

#include <utils/common/qnid.h>
#include <core/resource/resource_fwd.h>

#include <business/business_fwd.h>
#include <business/business_action_parameters.h>
#include <business/business_event_parameters.h>

namespace BusinessActionType
{
    QString toString( Value val );

    //TODO: #GDM fix to resourceTypeRequired: None, Camera, Server, User, etc
    bool requiresCameraResource(Value val);
    bool requiresUserResource(Value val);

    bool hasToggleState(Value val);
}


/**
 * Base class for business actions
 */
class QnAbstractBusinessAction
{
protected:

    explicit QnAbstractBusinessAction(const BusinessActionType::Value actionType, const QnBusinessEventParameters& runtimeParams);

public:
    virtual ~QnAbstractBusinessAction();
    BusinessActionType::Value actionType() const { return m_actionType; }

    /**
     * Resource depend of action type.
     * see: BusinessActionType::requiresCameraResource()
     * see: BusinessActionType::requiresUserResource()
     */
    void setResources(const QnResourceList& resources);

    const QnResourceList& getResources() const;

    void setParams(const QnBusinessActionParameters& params);
    const QnBusinessActionParameters& getParams() const;

    void setRuntimeParams(const QnBusinessEventParameters& params);
    const QnBusinessEventParameters& getRuntimeParams() const;

    void setBusinessRuleId(const QnId& value);
    QnId getBusinessRuleId() const;

    void setToggleState(Qn::ToggleState value);
    Qn::ToggleState getToggleState() const;

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
    BusinessActionType::Value m_actionType;
    Qn::ToggleState m_toggleState;
    bool m_receivedFromRemoteHost;
    QnResourceList m_resources;
    QnBusinessActionParameters m_params;
    QnBusinessEventParameters m_runtimeParams;
    QnId m_businessRuleId; // business rule that generated this action
    int m_aggregationCount;
};


class QnBusinessActionData
{
public:
    enum Flags {
        MotionExists = 1
    };

    QnBusinessActionData(): m_actionType(BusinessActionType::NotDefined), m_flags(0) {}

    BusinessActionType::Value actionType() const { return m_actionType; }
    void setActionType(BusinessActionType::Value type) { m_actionType = type; }

    //void setParams(const QnBusinessActionParameters& params) { m_params = params;}
    //const QnBusinessActionParameters& getParams() const { return m_params; }

    void setRuntimeParams(const QnBusinessEventParameters& params) {m_runtimeParams = params;}
    const QnBusinessEventParameters& getRuntimeParams() const {return m_runtimeParams; }

    void setBusinessRuleId(const QnId& value) {m_businessRuleId = value; }
    QnId getBusinessRuleId() const { return m_businessRuleId; }

    void setAggregationCount(int value) { m_aggregationCount = value; }
    int getAggregationCount() const { return m_aggregationCount; }

    qint64 timestamp() const { return m_runtimeParams.getEventTimestamp(); }
    BusinessEventType::Value eventType() const { return m_runtimeParams.getEventType(); }

    void setCompareString(const QString& value) { m_compareString = value; }
    const QString& compareString() const { return m_compareString; }

    void setFlags(int value) { m_flags = value; }
    int flags() const { return m_flags; }
    bool hasFlags(int flags) const { return m_flags & flags; }

protected:
    BusinessActionType::Value m_actionType;
    //QnBusinessActionParameters m_params;
    QnBusinessEventParameters m_runtimeParams;
    QnId m_businessRuleId; 
    int m_aggregationCount;
    QString m_compareString;
    int m_flags;
};

Q_DECLARE_METATYPE(BusinessActionType::Value)
Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)
Q_DECLARE_METATYPE(QnAbstractBusinessActionList)
Q_DECLARE_METATYPE(QnBusinessActionDataList)
Q_DECLARE_METATYPE(QnBusinessActionDataListPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
