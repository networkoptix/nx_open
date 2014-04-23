#ifndef __EC2__BUSINESS_RULE_DATA_H_
#define __EC2__BUSINESS_RULE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

class QnResourcePool;

namespace ec2
{
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


    struct ApiBusinessActionData: ApiData
    {
        qint32 actionType;
        Qn::ToggleState toggleState;
        bool receivedFromRemoteHost;
        std::vector<QnId> resources;
        QByteArray params;
        QByteArray runtimeParams;
        QnId businessRuleId;
        qint32 aggregationCount;

        /*void fromResource(const QnAbstractBusinessActionPtr& resource);
        QnAbstractBusinessActionPtr toResource(QnResourcePool* resourcePool) const;*/
    };
#define ApiBusinessActionData_Fields (actionType)(toggleState)(receivedFromRemoteHost)(resources)(params)(runtimeParams)(businessRuleId)(aggregationCount)

    /*struct ApiResetBusinessRuleData: public ApiData
    {
        ApiBusinessRuleList defaultRules;
    };*/


    /*struct ApiBusinessRule: ApiBusinessRuleData
    {
        void toResource(QnBusinessEventRulePtr resource, QnResourcePool* resourcePool) const;
        void fromResource(const QnBusinessEventRulePtr& resource);
        QN_DECLARE_STRUCT_SQL_BINDER();

    };
    //QN_DEFINE_STRUCT_SQL_BINDER(ApiBusinessRule, ApiBusinessRuleFields);

    struct ApiBusinessRuleList: std::vector<ApiBusinessRule>
    {
        void loadFromQuery(QSqlQuery& query);
        QnBusinessEventRuleList toResourceList(QnResourcePool* resourcePool) const;
        void fromResourceList(const QnBusinessEventRuleList& inData);
    };

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiBusinessActionData, ApiBusinessActionDataFields )
    QN_FUSION_DECLARE_FUNCTIONS(ApiBusinessActionData, (binary))

    struct ApiResetBusinessRuleData: public ApiData
    {
        ApiBusinessRuleList defaultRules;
    };

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiResetBusinessRuleData, (defaultRules) )
    QN_FUSION_DECLARE_FUNCTIONS(ApiResetBusinessRuleData, (binary))
    */
}

#endif // __EC2__BUSINESS_RULE_DATA_H_
