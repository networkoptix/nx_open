#include <QList>
#include "business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "toggle_business_event.h"

QnBusinessRuleProcessor* QnBusinessRuleProcessor::m_instance = 0;

QnBusinessRuleProcessor::QnBusinessRuleProcessor()
{
    connect(qnBusinessMessageBus, SIGNAL(actionDelivered(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDelivered(QnAbstractBusinessActionPtr)));
    connect(qnBusinessMessageBus, SIGNAL(actionDeliveryFail(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDeliveryFailed(QnAbstractBusinessActionPtr)));

    connect(qnBusinessMessageBus, SIGNAL(actionReceived(QnAbstractBusinessActionPtr)), this, SLOT(executeAction(QnAbstractBusinessActionPtr)));
}

QnBusinessRuleProcessor::~QnBusinessRuleProcessor()
{

}

QnMediaServerResourcePtr QnBusinessRuleProcessor::getDestMServer(QnAbstractBusinessActionPtr action)
{
    if (action->actionType() ==  BA_SendMail || action->actionType() ==  BA_Alert)
        return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
    if (!action->getResource())
        return QnMediaServerResourcePtr(); // can not find routeTo resource
    return action->getResource()->getParentResource().dynamicCast<QnMediaServerResource>();
}

void QnBusinessRuleProcessor::executeAction(QnAbstractBusinessActionPtr action)
{
    QnMediaServerResourcePtr routeToServer = getDestMServer(action);
    if (routeToServer && routeToServer->getGuid() != getGuid())
        qnBusinessMessageBus->deliveryBusinessAction(action, closeDirPath(routeToServer->getApiUrl()) + QLatin1String("api/execAction")); // delivery to other server
    else
        executeActionInternal(action);
}

bool QnBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action)
{
    switch(action->actionType())
    {
    case BA_SendMail:
        break;
    case BA_Alert:
        break;
    case BA_ShowPopup:
        break;

    default:
        break;
    }

    return false;
};

QnBusinessRuleProcessor* QnBusinessRuleProcessor::instance()
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(m_instance, Q_FUNC_INFO, "QnBusinessRuleProcessor::init must be called first!");
    return m_instance;
}

void QnBusinessRuleProcessor::init(QnBusinessRuleProcessor* instance)
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(!m_instance, Q_FUNC_INFO, "QnBusinessRuleProcessor::init must be called once!");
    m_instance = instance;
}

void QnBusinessRuleProcessor::addBusinessRule(QnBusinessEventRulePtr value)
{
    m_rules << value;
}

void QnBusinessRuleProcessor::processBusinessEvent(QnAbstractBusinessEventPtr bEvent)
{
    QList<QnAbstractBusinessActionPtr> actions = matchActions(bEvent);
    foreach(QnAbstractBusinessActionPtr action, actions)
    {
        executeAction(action);
    }
}

QList<QnAbstractBusinessActionPtr> QnBusinessRuleProcessor::matchActions(QnAbstractBusinessEventPtr bEvent)
{
    QList<QnAbstractBusinessActionPtr> result;
    foreach(QnBusinessEventRulePtr rule, m_rules)    
    {
        bool typeOK = rule->getEventType() == bEvent->getEventType();
        bool resOK = !rule->getSrcResource() || !bEvent->getResource() || rule->getSrcResource()->getId() == bEvent->getResource()->getId();
        if (typeOK && resOK)
        {
            bool condOK = bEvent->checkCondition(rule->getEventCondition());
            if (rule->isActionInProgress())
            {
                QnToggleBusinessEventPtr toggleEvent = bEvent.dynamicCast<QnToggleBusinessEvent>();
                // Toggle event repeated with some interval with state 'on'.
                if (!condOK)
                    result << rule->getAction(bEvent, ToggleState_Off); // if toggled action is used and condition is no longer valid - stop action
                else if (toggleEvent->getToggleState() == ToggleState_Off)
                    result << rule->getAction(bEvent); // Toggle event goes to 'off'. stop action
            }
            else if (condOK)
                result << rule->getAction(bEvent);
        }
    }
    return result;
}

void QnBusinessRuleProcessor::at_actionDelivered(QnAbstractBusinessActionPtr action)
{
    // todo: implement me
}

void QnBusinessRuleProcessor::at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action)
{
    // todo: implement me
}
