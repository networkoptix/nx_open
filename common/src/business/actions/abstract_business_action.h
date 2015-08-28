#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <utils/common/id.h>
#include <core/resource/resource_fwd.h>

#include <business/business_fwd.h>
#include <business/business_action_parameters.h>
#include <business/business_event_parameters.h>
#include <utils/common/model_functions_fwd.h>

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


struct QnBusinessActionData
{
public:
    // TODO: #EC2 Add comments. Maybe remove the flag altogether. What is it for? Which actions?
    enum Flags {
        MotionExists = 1
    };

    QnBusinessActionData(): actionType(QnBusiness::UndefinedAction), flags(0) {}
    bool hasFlags(int flags) const { return flags & flags; }

    QnBusiness::ActionType actionType;
    QnBusinessActionParameters actionParams;
    QnBusinessEventParameters eventParams;
    QnUuid businessRuleId; 
    int aggregationCount;

    int flags;
    QString compareString; // todo: this string is used on a client side for internal purpose. Need to move it to separate class
};

#define QnBusinessActionData_Fields (actionType)(actionParams)(eventParams)(businessRuleId)(aggregationCount)(flags)
QN_FUSION_DECLARE_FUNCTIONS(QnBusinessActionData, (ubjson)(json)(xml)(csv_record));

Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)
Q_DECLARE_METATYPE(QnAbstractBusinessActionList)
Q_DECLARE_METATYPE(QnBusinessActionDataList)
Q_DECLARE_METATYPE(QnBusinessActionDataListPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
