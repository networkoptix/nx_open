
#include <QList>
#include "business_rule_processor.h"
#include "core/resource/resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "events/business_event_rule.h"
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
    if (routeToServer && !action->isReceivedFromRemoteHost() /*&& routeToServer->getGuid() != getGuid()*/)
        qnBusinessMessageBus->deliveryBusinessAction(action, closeDirPath(routeToServer->getApiUrl()) + QLatin1String("api/execAction")); // delivery to other server
    else
        executeActionInternal(action);
}

static const QLatin1String RELAY_OUTPUT_ID( "relayOutputID" );
static const QLatin1String RELAY_AUTO_RESET_TIMEOUT( "relayAutoResetTimeout" );

bool QnBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action)
{
    switch( action->actionType() )
    {
        case BusinessActionType::BA_CameraOutput:
            return triggerCameraOutput( action );

        case BusinessActionType::BA_SendMail:
            return sendMail( action );

        case BusinessActionType::BA_Alert:
            break;

        case BusinessActionType::BA_ShowPopup:
            break;

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
        bool typeOK = rule->eventType() == bEvent->getEventType();
        bool resOK = !rule->eventResource() || !bEvent->getResource() || rule->eventResource()->getId() == bEvent->getResource()->getId();
        if (typeOK && resOK)
        {
            bool condOK = bEvent->checkCondition(rule->eventParams());
            QnAbstractBusinessActionPtr action;

            if (m_rulesInProgress.contains(rule->getUniqueId()))
            {
                // Toggle event repeated with some interval with state 'on'.
                if (!condOK)
                    action = rule->instantiateAction(bEvent, ToggleState::Off); // if toggled action is used and condition is no longer valid - stop action
                else if (bEvent->getToggleState() == ToggleState::Off)
                    action = rule->instantiateAction(bEvent); // Toggle event goes to 'off'. stop action
            }
            else if (condOK)
                action = rule->instantiateAction(bEvent);

            if (action)
                result << action;

            bool actionInProgress = action && action->getToggleState() == ToggleState::On;
            if (actionInProgress)
                m_rulesInProgress.insert(rule->getUniqueId());
            else
                m_rulesInProgress.remove(rule->getUniqueId());
        }
    }
    return result;
}

void QnBusinessRuleProcessor::at_actionDelivered(QnAbstractBusinessActionPtr action)
{
    //TODO: implement me
}

void QnBusinessRuleProcessor::at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action)
{
    //TODO: implement me
}

bool QnBusinessRuleProcessor::triggerCameraOutput( const QnAbstractBusinessActionPtr& action )
{
    const QnResourcePtr& resource = action->getResource();

    if( !resource )
    {
        cl_log.log( QString::fromLatin1("Received BA_CameraOutput with no resource reference. Ignoring..."), cl_logWARNING );
        return false;
    }
    QnSecurityCamResourcePtr securityCam = resource.dynamicCast<QnSecurityCamResource>();
    if( !securityCam )
    {
        cl_log.log( QString::fromLatin1("Received BA_CameraOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
            arg(resource->getId()), cl_logWARNING );
        return false;
    }
    QnBusinessParams::const_iterator relayOutputIDIter = action->getParams().find( RELAY_OUTPUT_ID );
    if( relayOutputIDIter == action->getParams().end() )
    {
        cl_log.log( QString::fromLatin1("Received BA_CameraOutput action without required parameter %1. Ignoring...").arg(RELAY_OUTPUT_ID), cl_logWARNING );
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

    QString emailAddress = BusinessActionParameters::getEmailAddress(action->getParams());
    if( emailAddress.isEmpty() )
    {
        cl_log.log( QString::fromLatin1("Action SendMail missing required parameter \"emailAddress\". Ignoring..."), cl_logWARNING );
        return false;
    }

    cl_log.log( QString::fromLatin1("Processing action SendMail. Sending mail to %1").
        arg(emailAddress), cl_logDEBUG1 );


    const QnAppServerConnectionPtr& appServerConnection = QnAppServerConnectionFactory::createConnection();
    QByteArray emailSendErrorText;
    if( appServerConnection->sendEmail(
            emailAddress,
            sendMailAction->getSubject(),
            sendMailAction->getMessageBody(),
            emailSendErrorText ) )
    {
        cl_log.log( QString::fromLatin1("Error processing action SendMail (TO: %1). %2").
            arg(emailAddress).arg(QLatin1String(emailSendErrorText)), cl_logWARNING );
        return false;
    }
    return true;
}
