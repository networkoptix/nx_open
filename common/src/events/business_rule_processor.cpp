
#include <QList>
#include "business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "api/app_server_connection.h"
#include "sendmail_business_action.h"


QnBusinessRuleProcessor* QnBusinessRuleProcessor::m_instance = 0;

QnBusinessRuleProcessor::QnBusinessRuleProcessor()
{
    connect(qnBusinessMessageBus, SIGNAL(actionDelivered(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDelivered(QnAbstractBusinessActionPtr)));
    connect(qnBusinessMessageBus, SIGNAL(actionDeliveryFail(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDeliveryFailed(QnAbstractBusinessActionPtr)));

    connect(qnBusinessMessageBus, SIGNAL(actionReceived(QnAbstractBusinessActionPtr)), this, SLOT(executeAction(QnAbstractBusinessActionPtr)));

    start();
}

QnBusinessRuleProcessor::~QnBusinessRuleProcessor()
{
}

QnMediaServerResourcePtr QnBusinessRuleProcessor::getDestMServer(QnAbstractBusinessActionPtr action)
{
    if (action->actionType() == BusinessActionType::BA_SendMail || action->actionType() == BusinessActionType::BA_Alert)
        return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
    if (!action->getResource())
        return QnMediaServerResourcePtr(); // can not find routeTo resource
    return action->getResource()->getParentResource().dynamicCast<QnMediaServerResource>();
}

void QnBusinessRuleProcessor::executeAction(QnAbstractBusinessActionPtr action)
{
    QnMediaServerResourcePtr routeToServer = getDestMServer(action);
    if (routeToServer && !action->isReceivedFromRemoveHost() /*&& routeToServer->getGuid() != getGuid()*/)
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
        case BusinessActionType::BA_SendMail:
            return sendMail( action );

        case BusinessActionType::BA_Alert:
            break;

        case BusinessActionType::BA_ShowPopup:
            break;

        case BusinessActionType::BA_TriggerOutput:
            return triggerCameraOutput( action );

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
            QnAbstractBusinessActionPtr action;
            if (rule->isActionInProgress())
            {
                // Toggle event repeated with some interval with state 'on'.
                if (!condOK)
                    action = rule->getAction(bEvent, ToggleState::Off); // if toggled action is used and condition is no longer valid - stop action
                else if (bEvent->getToggleState() == ToggleState::Off)
                    action = rule->getAction(bEvent); // Toggle event goes to 'off'. stop action
            }
            else if (condOK)
                action = rule->getAction(bEvent);
            if (action)
                result << action;
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

bool QnBusinessRuleProcessor::triggerCameraOutput( const QnAbstractBusinessActionPtr& action )
{
    const QnResourcePtr& resource = action->getResource();

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
        action->getToggleState() == ToggleState::On,
        autoResetTimeoutIter == action->getParams().end() ? 0 : autoResetTimeoutIter->toUInt() );
}

bool QnBusinessRuleProcessor::sendMail( const QnAbstractBusinessActionPtr& action )
{
    QnSendMailBusinessActionPtr sendMailAction = action.dynamicCast<QnSendMailBusinessAction>();
    Q_ASSERT( sendMailAction );

    const QnBusinessParams& actionParams = sendMailAction->getParams();
    QnBusinessParams::const_iterator emailIter = actionParams.find( BusinessActionParamName::emailAddress );
    if( emailIter == actionParams.end() )
    {
        cl_log.log( QString::fromLatin1("Action SendMail missing required parameter \"%1\". Ignoring...").
            arg(BusinessActionParamName::emailAddress), cl_logWARNING );
        return false;
    }

    cl_log.log( QString::fromLatin1("Processing action SendMail from camera %1. Sending mail to %2").
        arg(sendMailAction->getEvent()->getResource()->getName()).arg(emailIter.value().toString()), cl_logDEBUG1 );

    const QnAppServerConnectionPtr& appServerConnection = QnAppServerConnectionFactory::createConnection();
    QByteArray emailSendErrorText;
    if( appServerConnection->sendEmail(
            emailIter.value().toString(),
            sendMailAction->getEvent()->getResource()
                ? QString::fromLatin1("Event %1 received from resource %2 (%3)").
                    arg(BusinessEventType::toString(sendMailAction->getEvent()->getEventType())).
                    arg(sendMailAction->getEvent()->getResource()->getName()).
                    arg(sendMailAction->getEvent()->getResource()->getUrl())
                : QString::fromLatin1("Event %1 received from resource UNKNOWN").
                    arg(BusinessEventType::toString(sendMailAction->getEvent()->getEventType())),
            sendMailAction->toString(),
            emailSendErrorText ) )
    {
        cl_log.log( QString::fromLatin1("Error processing action SendMail (TO: %1). %2").
            arg(emailIter.value().toString()).arg(QLatin1String(emailSendErrorText)), cl_logWARNING );
        return false;
    }
    return true;
}
