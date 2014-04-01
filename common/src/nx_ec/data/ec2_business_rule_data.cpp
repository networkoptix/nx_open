#include "ec2_business_rule_data.h"
#include "core/resource_management/resource_pool.h"
#include "business/business_action_parameters.h"
#include "business/business_action_factory.h"

namespace ec2
{

void ApiBusinessRule::toResource(QnBusinessEventRulePtr resource, QnResourcePool* resourcePool) const
{
    resource->setId(id);
    resource->setEventType(eventType);
    
    resource->setEventResources(QVector<QnId>::fromStdVector(eventResource));
    
    QnBusinessParams bParams = deserializeBusinessParams(eventCondition);
    resource->setEventParams(QnBusinessEventParameters::fromBusinessParams(bParams));

    resource->setEventState(eventState);
    resource->setActionType(actionType);

    resource->setActionResources(QVector<QnId>::fromStdVector(actionResource));

    bParams = deserializeBusinessParams(actionParams);
    resource->setActionParams(QnBusinessActionParameters::fromBusinessParams(bParams));

    resource->setAggregationPeriod(aggregationPeriod);
    resource->setDisabled(disabled);
    resource->setComments(comments);
    resource->setSchedule(schedule);
    resource->setSystem(system);
}

void ApiBusinessRule::fromResource(const QnBusinessEventRulePtr& resource)
{
    id = resource->id();
    eventType = resource->eventType();

    eventResource = resource->eventResources().toStdVector();
    actionResource = resource->actionResources().toStdVector();

    eventCondition = serializeBusinessParams(resource->eventParams().toBusinessParams());
    actionParams = serializeBusinessParams(resource->actionParams().toBusinessParams());

    eventState = resource->eventState();
    actionType = resource->actionType();
    aggregationPeriod = resource->aggregationPeriod();
    disabled = resource->disabled();
    comments = resource->comments();
    schedule = resource->schedule();
    system = resource->system();
}

QnBusinessEventRuleList ApiBusinessRuleList::toResourceList(QnResourcePool* resourcePool) const
{
    QnBusinessEventRuleList outData;
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnBusinessEventRulePtr rule(new QnBusinessEventRule());
        data[i].toResource(rule, resourcePool);
        outData << rule;
    }
    return outData;
}

void ApiBusinessRuleList::fromResourceList(const QnBusinessEventRuleList& inData)
{
    data.reserve(inData.size());
    foreach(const QnBusinessEventRulePtr& bRule, inData) {
        data.push_back(ApiBusinessRule());
        data.back().fromResource(bRule);
    }
}

void ApiBusinessRuleList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiBusinessRule, data, ApiBusinessRuleFields)
}

void ApiBusinessActionData::fromResource(const QnAbstractBusinessActionPtr& resource)
{
    actionType = resource->actionType();
    toggleState = resource->getToggleState();
    receivedFromRemoteHost = resource->isReceivedFromRemoteHost();
    resources = resource->getResources().toStdVector();

    params = serializeBusinessParams(resource->getParams().toBusinessParams());
    runtimeParams = serializeBusinessParams(resource->getRuntimeParams().toBusinessParams());

    businessRuleId = resource->getBusinessRuleId();
    aggregationCount = resource->getAggregationCount();
}

QnAbstractBusinessActionPtr  ApiBusinessActionData::toResource(QnResourcePool* resourcePool) const
{
    QnBusinessParams bParams = deserializeBusinessParams(runtimeParams);
    QnAbstractBusinessActionPtr resource = QnBusinessActionFactory::createAction((BusinessActionType::Value) actionType, QnBusinessEventParameters::fromBusinessParams(bParams));

    resource->setToggleState(toggleState);
    resource->setReceivedFromRemoteHost(receivedFromRemoteHost);

    resource->setResources(QVector<QnId>::fromStdVector(resources));

    bParams = deserializeBusinessParams(params);
    resource->setParams(QnBusinessActionParameters::fromBusinessParams(bParams));

    resource->setBusinessRuleId(businessRuleId);
    resource->setAggregationCount(aggregationCount);

    return resource;
}

}
