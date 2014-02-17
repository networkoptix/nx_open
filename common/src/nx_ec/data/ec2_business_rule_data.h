#ifndef __EC2__BUSINESS_RULE_DATA_H_
#define __EC2__BUSINESS_RULE_DATA_H_

#include "core/resource/resource_fwd.h"
#include "business/business_fwd.h"
#include "business/business_event_rule.h"
#include "api_data.h"

class QnResourcePool;

namespace ec2
{
    
struct ApiBusinessRuleData: public ApiData
{
    ApiBusinessRuleData(): 
        id(0), eventType(BusinessEventType::NotDefined), eventState(Qn::UndefinedState), actionType(BusinessActionType::NotDefined), 
        aggregationPeriod(0), disabled(false), system(false) {}

    qint32 id;

    BusinessEventType::Value eventType;
    std::vector<qint32>  eventResource;
    QByteArray eventCondition;
    Qn::ToggleState eventState;
    
    BusinessActionType::Value actionType;
    std::vector<qint32> actionResource;
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
    void toResourceList(QnBusinessEventRuleList& outData, QnResourcePool* resourcePool) const;
};

struct ApiBusinessActionData: public ApiData
{
    qint32 actionType;
    Qn::ToggleState toggleState;
    bool receivedFromRemoteHost;
    std::vector<qint32> resources;
    QByteArray params;
    QByteArray runtimeParams;
    qint32 businessRuleId;
    qint32 aggregationCount;
    
    void fromResource(const QnAbstractBusinessActionPtr& resource);
    QnAbstractBusinessActionPtr toResource(QnResourcePool* resourcePool) const;
};

}

#define ApiBusinessRuleFields (id) (eventType) (eventResource) (eventCondition) (eventState) (actionType) (actionResource) (actionParams) (aggregationPeriod) (disabled) (comments) (schedule) (system)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiBusinessRuleData, ApiBusinessRuleFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiBusinessRuleDataList, (data) )
#define ApiBusinessActionDataFields (actionType) (toggleState) (receivedFromRemoteHost) (resources) (params) (runtimeParams) (businessRuleId) (aggregationCount)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiBusinessActionData, ApiBusinessActionDataFields )


#endif // __EC2__BUSINESS_RULE_DATA_H_
