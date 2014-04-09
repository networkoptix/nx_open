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
    struct ApiBusinessRule;
    #include "ec2_business_rule_data_i.h"

    struct ApiBusinessRule: ApiBusinessRuleData
    {
        void toResource(QnBusinessEventRulePtr resource, QnResourcePool* resourcePool) const;
        void fromResource(const QnBusinessEventRulePtr& resource);
        QN_DECLARE_STRUCT_SQL_BINDER();

    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiBusinessRule, ApiBusinessRuleFields);
}


namespace ec2
{
    struct ApiBusinessRuleList: public ApiBusinessRuleListData
    {
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

    #define ApiBusinessActionDataFields (actionType) (toggleState) (receivedFromRemoteHost) (resources) (params) (runtimeParams) (businessRuleId) (aggregationCount)
    QN_DEFINE_STRUCT_SERIALIZATORS (ApiBusinessActionData, ApiBusinessActionDataFields )

    struct ApiResetBusinessRuleData: public ApiData
    {
        ApiBusinessRuleList defaultRules;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiResetBusinessRuleData, (defaultRules) )
}

#endif // __EC2__BUSINESS_RULE_DATA_H_
