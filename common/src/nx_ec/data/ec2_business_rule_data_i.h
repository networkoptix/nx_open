#ifndef QN_BUSINESS_RULE_DATA_I_H
#define QN_BUSINESS_RULE_DATA_I_H

#include "api_data_i.h"

namespace ec2 {

    struct ApiBusinessRuleData: ApiData {
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
    };

    #define ApiBusinessRuleData_Fields (id)(eventType)(eventResource)(eventCondition)(eventState)(actionType)(actionResource)(actionParams) \
                                    (aggregationPeriod)(disabled)(comments)(schedule)(system)

} // namespace ec2

#endif // QN_BUSINESS_RULE_DATA_I_H
