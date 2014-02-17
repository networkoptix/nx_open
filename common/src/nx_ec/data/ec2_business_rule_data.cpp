#include "ec2_business_rule_data.h"
#include "core/resource_management/resource_pool.h"
#include "business/business_action_parameters.h"

namespace ec2
{

void ApiBusinessRuleData::toResource(QnBusinessEventRulePtr resource, QnResourcePool* resourcePool) const
{
    resource->setId(id);
    resource->setEventType(eventType);
    
    QnResourceList resList;
    foreach(qint32 resId, eventResource) {
        QnResourcePtr res = resourcePool->getResourceById(resId);
        if (res)
            resList << res;
    }
    resource->setEventResources(resList);
    
    QnBusinessParams bParams = deserializeBusinessParams(eventCondition);
    resource->setEventParams(QnBusinessEventParameters::fromBusinessParams(bParams));

    resource->setEventState(eventState);
    resource->setActionType(actionType);

    resList.clear();
    foreach(qint32 resId, actionResource) {
        QnResourcePtr res = resourcePool->getResourceById(resId);
        if (res)
            resList << res;
    }
    resource->setActionResources(resList);

    bParams = deserializeBusinessParams(actionParams);
    resource->setActionParams(QnBusinessActionParameters::fromBusinessParams(bParams));

    resource->setAggregationPeriod(aggregationPeriod);
    resource->setDisabled(disabled);
    resource->setComments(comments);
    resource->setSchedule(schedule);
    resource->setSystem(system);
}

void ApiBusinessRuleData::fromResource(const QnBusinessEventRulePtr& resource)
{
    id = resource->id();
    eventType = resource->eventType();

    foreach(const QnResourcePtr& res,  resource->eventResources())
        eventResource.push_back(res->getId());
    foreach(const QnResourcePtr& res,  resource->actionResources())
        actionResource.push_back(res->getId());

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

void ApiBusinessRuleDataList::toResourceList(QnBusinessEventRuleList& outData, QnResourcePool* resourcePool) const
{
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnBusinessEventRulePtr rule(new QnBusinessEventRule());
        data[i].toResource(rule, resourcePool);
        outData << rule;
    }
}

void ApiBusinessRuleDataList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiBusinessRuleData, data, ApiBusinessRuleFields)
}

}
