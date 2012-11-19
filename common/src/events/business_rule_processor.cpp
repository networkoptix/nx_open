
#include <QList>
#include "business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"


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

static const QLatin1String RELAY_OUTPUT_ID( "relayOutputID" );
static const QLatin1String RELAY_AUTO_RESET_TIMEOUT( "relayAutoResetTimeout" );

bool QnBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action)
{
    QnResourcePtr resource = action->getResource();

    switch( action->actionType() )
    {
        case BA_SendMail:
            break;
        case BA_Alert:
            break;
        case BA_ShowPopup:
            break;

        case BA_TriggerOutput:
        {
            if( !resource )
            {
                cl_log.log( QString::fromLatin1("Received BA_TriggerOutput with no resource reference. Ignoring..."), cl_logWARNING );
                return false;
            }
            QnSecurityCamResourcePtr securityCam = resource.dynamicCast<QnSecurityCamResource>();
            if( !securityCam )
            {
                cl_log.log( QString::fromLatin1("Received BA_TriggerOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
                    arg(resource->getId()), cl_logWARNING );
                return false;
            }
            QnBusinessParams::const_iterator relayOutputIDIter = action->getParams().find( RELAY_OUTPUT_ID );
            if( relayOutputIDIter == action->getParams().end() )
            {
                cl_log.log( QString::fromLatin1("Received BA_TriggerOutput action without required parameter %1. Ignoring...").arg(RELAY_OUTPUT_ID), cl_logWARNING );
                return false;
            }
            QnBusinessParams::const_iterator autoResetTimeoutIter = action->getParams().find( RELAY_AUTO_RESET_TIMEOUT );
            return securityCam->setRelayOutputState(
                relayOutputIDIter->toString(),
                action->getToggleState() == ToggleState_On,
                autoResetTimeoutIter == action->getParams().end() ? 0 : autoResetTimeoutIter->toUInt() );
        }

        default:
            break;
    }

    return false;
}

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
                // Toggle event repeated with some interval with state 'on'.
                if (!condOK)
                    result << rule->getAction(bEvent, ToggleState_Off); // if toggled action is used and condition is no longer valid - stop action
                else if (bEvent->getToggleState() == ToggleState_Off)
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
