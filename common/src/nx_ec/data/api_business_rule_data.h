#ifndef __EC2__BUSINESS_RULE_DATA_H_
#define __EC2__BUSINESS_RULE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

class QnResourcePool;

namespace ec2
{
    struct ApiBusinessRuleData: ApiIdData {
        ApiBusinessRuleData():
            eventType(nx::vms::event::undefinedEvent), eventState(nx::vms::event::EventState::undefined), actionType(nx::vms::event::undefinedAction),
            aggregationPeriod(0), disabled(false), system(false) {}

        nx::vms::event::EventType eventType;
        std::vector<QnUuid>  eventResourceIds;
        QnLatin1Array eventCondition;
        nx::vms::event::EventState eventState;

        nx::vms::event::ActionType actionType;
        std::vector<QnUuid> actionResourceIds;
        QnLatin1Array actionParams;

        qint32 aggregationPeriod; //< Seconds.
        bool disabled;
        QString comment;
        QString schedule;

        bool system; // system rule cannot be deleted
    };
#define ApiBusinessRuleData_Fields ApiIdData_Fields (eventType)(eventResourceIds)(eventCondition)(eventState)(actionType)(actionResourceIds)(actionParams) \
    (aggregationPeriod)(disabled)(comment)(schedule)(system)


    struct ApiBusinessActionData: ApiData
    {
        ApiBusinessActionData(): ApiData(), actionType(nx::vms::event::undefinedAction), toggleState(nx::vms::event::EventState::undefined), receivedFromRemoteHost(false), aggregationCount(0) {}

        nx::vms::event::ActionType actionType;
        nx::vms::event::EventState toggleState;
        bool receivedFromRemoteHost;
        std::vector<QnUuid> resourceIds;
        QByteArray params;
        QByteArray runtimeParams;
        QnUuid ruleId;
        qint32 aggregationCount;
    };
#define ApiBusinessActionData_Fields (actionType)(toggleState)(receivedFromRemoteHost)(resourceIds)(params)(runtimeParams)(ruleId)(aggregationCount)


    struct ApiResetBusinessRuleData: ApiData
    {
        // TODO: #rvasilenko these rules are not used right now. Cannot remove as this type is used
        // to deduct transaction type by template substitution.
        ApiBusinessRuleDataList defaultRules;
    };
#define ApiResetBusinessRuleData_Fields (defaultRules)

} // namespace ec2

#endif // __EC2__BUSINESS_RULE_DATA_H_
