#ifndef __EC2__BUSINESS_RULE_DATA_H_
#define __EC2__BUSINESS_RULE_DATA_H_

#include "core/resource/resource_fwd.h"
#include "business/business_fwd.h"
#include "business/business_event_rule.h"
#include "api_data.h"
#include "utils/common/id.h"

class QnResourcePool;

namespace ec2
{
    
struct ApiBusinessRuleData: public ApiData
{
    ApiBusinessRuleData(): 
        eventType(BusinessEventType::NotDefined), eventState(Qn::UndefinedState), actionType(BusinessActionType::NotDefined), 
        aggregationPeriod(0), disabled(false), system(false) {}

    QnId id;

    BusinessEventType::Value eventType;
    std::vector<QnId>  eventResource;
    QByteArray eventCondition;
    Qn::ToggleState eventState;
    
    BusinessActionType::Value actionType;
    std::vector<QnId> actionResource;
    QByteArray actionParams;

    qint32 aggregationPeriod; // msecs
    bool disabled;
    QString comments;
    QString schedule;

    bool system; // system rule cannot be deleted 

    void toResource(QnBusinessEventRulePtr resource, QnResourcePool* resourcePool) const;
    void fromResource(const QnBusinessEventRulePtr& resource);
    QN_DECLARE_STRUCT_SQL_BINDER();

};

struct ApiBusinessRuleDataList: public ApiData
{
    std::vector<ApiBusinessRuleData> data;
    
    void loadFromQuery(QSqlQuery& query);
    QnBusinessEventRuleList toResourceList(QnResourcePool* resourcePool) const;
    void fromResourceList(const QnBusinessEventRuleList& inData);
};

struct ApiBusinessActionData: public ApiData
{
    qint32 actionType;
    Qn::ToggleState toggleState;
    bool receivedFromRemoteHost;
    std::vector<QnId> resources;
    QByteArray params;
    QByteArray runtimeParams;
    QnId businessRuleId;
    qint32 aggregationCount;
    
    void fromResource(const QnAbstractBusinessActionPtr& resource);
    QnAbstractBusinessActionPtr toResource(QnResourcePool* resourcePool) const;
};

struct ApiResetBusinessRuleData: public ApiData
{
    ApiBusinessRuleDataList defaultRules;
};

}

#define ApiBusinessRuleFields (id) (eventType) (eventResource) (eventCondition) (eventState) (actionType) (actionResource) (actionParams) (aggregationPeriod) (disabled) (comments) (schedule) (system)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiBusinessRuleData, ApiBusinessRuleFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiBusinessRuleDataList, (data) )
#define ApiBusinessActionDataFields (actionType) (toggleState) (receivedFromRemoteHost) (resources) (params) (runtimeParams) (businessRuleId) (aggregationCount)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiBusinessActionData, ApiBusinessActionDataFields )
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiResetBusinessRuleData, (defaultRules) )


#endif // __EC2__BUSINESS_RULE_DATA_H_
