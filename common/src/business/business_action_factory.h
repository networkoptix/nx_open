#ifndef BUSINESS_ACTION_FACTORY_H
#define BUSINESS_ACTION_FACTORY_H

#include <business/actions/abstract_business_action.h>
#include <business/business_aggregation_info.h>
#include <business/business_event_rule.h>

class QnBusinessActionFactory
{
public:
    static QnAbstractBusinessActionPtr instantiateAction(
        const QnBusinessEventRulePtr &rule,
        const QnAbstractBusinessEventPtr &event,
        const QnUuid& moduleGuid,
        QnBusiness::EventState state = QnBusiness::UndefinedState);

    static QnAbstractBusinessActionPtr instantiateAction(
        const QnBusinessEventRulePtr &rule,
        const QnAbstractBusinessEventPtr &event,
        const QnUuid& moduleGuid,
        const QnBusinessAggregationInfo& aggregationInfo);

    static QnAbstractBusinessActionPtr createAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters &runtimeParams);

    static QnAbstractBusinessActionPtr cloneAction(QnAbstractBusinessActionPtr action);
};

#endif // BUSINESS_ACTION_FACTORY_H
