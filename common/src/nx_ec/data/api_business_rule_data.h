#ifndef __EC2__BUSINESS_RULE_DATA_H_
#define __EC2__BUSINESS_RULE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

class QnResourcePool;

namespace ec2
{
    struct ApiBusinessRuleData: ApiData {
        ApiBusinessRuleData(): 
            eventType(QnBusiness::UndefinedEvent), eventState(QnBusiness::UndefinedState), actionType(QnBusiness::UndefinedAction), 
            aggregationPeriod(0), disabled(false), system(false) {}

        QnUuid id;

        QnBusiness::EventType eventType;
        std::vector<QnUuid>  eventResourceIds;
        QByteArray eventCondition; 
        QnBusiness::EventState eventState;

        QnBusiness::ActionType actionType;
        std::vector<QnUuid> actionResourceIds;
        QByteArray actionParams;

        qint32 aggregationPeriod; // msecs
        bool disabled;
        QString comment;
        QString schedule;

        bool system; // system rule cannot be deleted 
    };
#define ApiBusinessRuleData_Fields (id)(eventType)(eventResourceIds)(eventCondition)(eventState)(actionType)(actionResourceIds)(actionParams) \
    (aggregationPeriod)(disabled)(comment)(schedule)(system)


    struct ApiBusinessActionData: ApiData
    {
        QnBusiness::ActionType actionType;
        QnBusiness::EventState toggleState;
        bool receivedFromRemoteHost;
        std::vector<QnUuid> resourceIds;
        QByteArray params;
        QByteArray runtimeParams;
        QnUuid ruleId;
        qint32 aggregationCount;
    };
#define ApiBusinessActionData_Fields (actionType)(toggleState)(receivedFromRemoteHost)(resourceIds)(params)(runtimeParams)(ruleId)(aggregationCount)

    
    struct ApiResetBusinessRuleData: public ApiData
    {
        ApiBusinessRuleDataList defaultRules;
    };
#define ApiResetBusinessRuleData_Fields (defaultRules)

} // namespace ec2

#endif // __EC2__BUSINESS_RULE_DATA_H_
