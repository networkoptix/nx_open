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

        QnId id;

        QnBusiness::EventType eventType;
        std::vector<QnId>  eventResource;
        QByteArray eventCondition;
        QnBusiness::EventState eventState;

        QnBusiness::ActionType actionType;
        std::vector<QnId> actionResource;
        QByteArray actionParams;

        qint32 aggregationPeriod; // msecs
        bool disabled;
        QString comments;
        QString schedule;

        bool system; // system rule cannot be deleted 
    };
#define ApiBusinessRuleData_Fields (id)(eventType)(eventResource)(eventCondition)(eventState)(actionType)(actionResource)(actionParams) \
    (aggregationPeriod)(disabled)(comments)(schedule)(system)


    struct ApiBusinessActionData: ApiData
    {
        qint32 actionType; // TODO: #Elric #EC2 QnBusiness::ActionType
        QnBusiness::EventState toggleState;
        bool receivedFromRemoteHost;
        std::vector<QnId> resources;
        QByteArray params;
        QByteArray runtimeParams;
        QnId businessRuleId;
        qint32 aggregationCount;
    };
#define ApiBusinessActionData_Fields (actionType)(toggleState)(receivedFromRemoteHost)(resources)(params)(runtimeParams)(businessRuleId)(aggregationCount)

    
    struct ApiResetBusinessRuleData: public ApiData
    {
        ApiBusinessRuleDataList defaultRules;
    };
#define ApiResetBusinessRuleData_Fields (defaultRules)

} // namespace ec2

#endif // __EC2__BUSINESS_RULE_DATA_H_
