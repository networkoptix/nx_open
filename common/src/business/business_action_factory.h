#ifndef BUSINESS_ACTION_FACTORY_H
#define BUSINESS_ACTION_FACTORY_H

#include <business/actions/abstract_business_action.h>
#include <business/business_aggregation_info.h>
#include <business/business_event_rule.h>

class QnBusinessActionFactory
{
public:
    static QnAbstractBusinessActionPtr instantiateAction(const QnBusinessEventRulePtr &rule,
                                                         const QnAbstractBusinessEventPtr &event,
                                                         ToggleState::Value state = ToggleState::NotDefined);

    static QnAbstractBusinessActionPtr instantiateAction(const QnBusinessEventRulePtr &rule,
                                                         const QnAbstractBusinessEventPtr &event,
                                                         const QnBusinessAggregationInfo& aggregationInfo);

    static QnAbstractBusinessActionPtr createAction(const BusinessActionType::Value actionType,
                                                    const QnBusinessParams &runtimeParams);
};

#endif // BUSINESS_ACTION_FACTORY_H
