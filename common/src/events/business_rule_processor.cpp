
#include <QList>
#include "business_rule_processor.h"
#include "core/resource/resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "events/business_event_rule.h"
#include "api/app_server_connection.h"
#include "utils/common/synctime.h"

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
    //TODO: #vasilenko resources list
//    if (!action->getResources())
        return QnMediaServerResourcePtr(); // can not find routeTo resource
//    return action->getResource()->getParentResource().dynamicCast<QnMediaServerResource>();
}

void QnBusinessRuleProcessor::executeAction(QnAbstractBusinessActionPtr action)
{
    QnMediaServerResourcePtr routeToServer = getDestMServer(action);
    if (routeToServer && !action->isReceivedFromRemoteHost() /*&& routeToServer->getGuid() != getGuid()*/)
        qnBusinessMessageBus->deliveryBusinessAction(action, closeDirPath(routeToServer->getApiUrl()) + QLatin1String("api/execAction")); // delivery to other server
    else
        executeActionInternal(action);
}

bool QnBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action)
{
    switch( action->actionType() )
    {
        case BusinessActionType::BA_CameraOutput:
            return triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>());

        case BusinessActionType::BA_SendMail:
            return sendMail( action.dynamicCast<QnSendMailBusinessAction>() );

        case BusinessActionType::BA_Alert:
            break;

        case BusinessActionType::BA_ShowPopup:
            return showPopup( action.dynamicCast<QnPopupBusinessAction>() );

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
    QMutexLocker lock(&m_mutex);
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

bool QnBusinessRuleProcessor::containResource(QnResourceList resList, const QnId& resId) const
{
    for (int i = 0; i < resList.size(); ++i)
    {
        if (resList.at(i)->getId() == resId)
            return true;
    }
    return false;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = bEvent->checkCondition(rule->eventState(), rule->eventParams());
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

    bool actionInProgress = action && action->getToggleState() == ToggleState::On;
    if (actionInProgress)
        m_rulesInProgress.insert(rule->getUniqueId());
    else
        m_rulesInProgress.remove(rule->getUniqueId());

    return action;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processInstantAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = bEvent->checkCondition(rule->eventState(), rule->eventParams());
    if (!condOK)
        return QnAbstractBusinessActionPtr();

    QString eventKey = rule->getUniqueId();
    if (bEvent->getResource())
        eventKey += bEvent->getResource()->getUniqueId();

    QAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    aggInfo.count++;
    if (currentTime - aggInfo.timeStamp < rule->aggregationPeriod()*1000ll*1000ll)
        return QnAbstractBusinessActionPtr();
    
    QnAbstractBusinessActionPtr action = rule->instantiateAction(bEvent);
    action->setAggregationCount(aggInfo.count);
    aggInfo.count = 0;
    return action;
}

QList<QnAbstractBusinessActionPtr> QnBusinessRuleProcessor::matchActions(QnAbstractBusinessEventPtr bEvent)
{
    QMutexLocker lock(&m_mutex);
    QList<QnAbstractBusinessActionPtr> result;
    foreach(QnBusinessEventRulePtr rule, m_rules)    
    {
        bool typeOK = rule->eventType() == bEvent->getEventType();
        bool resOK = rule->eventResources().isEmpty() || !bEvent->getResource() || containResource(rule->eventResources(), bEvent->getResource()->getId());
        if (typeOK && resOK)
        {
            QnAbstractBusinessActionPtr action;

            if (BusinessActionType::hasToggleState(rule->actionType()))
                action = processToggleAction(bEvent, rule);
            else
                action = processInstantAction(bEvent, rule);

            if (action)
                result << action;
        }
    }
    return result;
}

void QnBusinessRuleProcessor::at_actionDelivered(QnAbstractBusinessActionPtr action)
{
    Q_UNUSED(action)
    //TODO: implement me
}

void QnBusinessRuleProcessor::at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action)
{
    Q_UNUSED(action)
    //TODO: implement me
}

//TODO: move to mserver_business_rule_processor
bool QnBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action )
{

    //TODO: #vasilenko resources list
//    const QnResourcePtr& resource = action->getResource();
    QnResourcePtr resource;

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
    QString relayOutputId = action->getRelayOutputId();
    if( relayOutputId.isEmpty() )
    {
        cl_log.log( QString::fromLatin1("Received BA_CameraOutput action without required parameter relayOutputID. Ignoring..."), cl_logWARNING );
        return false;
    }

    int autoResetTimeout = qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    return securityCam->setRelayOutputState(
        relayOutputId,
        action->getToggleState() == ToggleState::On,
        autoResetTimeout );
}

bool QnBusinessRuleProcessor::sendMail( const QnSendMailBusinessActionPtr& action )
{
    Q_ASSERT( action );

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
            action->getSubject(),
            action->getMessageBody()))
    {
        cl_log.log( QString::fromLatin1("Error processing action SendMail (TO: %1). %2").
            arg(emailAddress).arg(QLatin1String(appServerConnection->getLastError())), cl_logWARNING );
        return false;
    }
    return true;
}

bool QnBusinessRuleProcessor::showPopup(QnPopupBusinessActionPtr action)
{
    const QnAppServerConnectionPtr& appServerConnection = QnAppServerConnectionFactory::createConnection();
    if( appServerConnection->broadcastBusinessAction(action))
    {
        qWarning() << "Error processing action broadcastBusinessAction";
        return false;
    }
    return true;
}

void QnBusinessRuleProcessor::at_businessRuleChanged(QnBusinessEventRulePtr bRule)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->getId() == bRule->getId())
        {
            m_rules[i] = bRule;
            return;
        }
    }
    m_rules << bRule;
}

void QnBusinessRuleProcessor::at_businessRuleDeleted(QnId id)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->getId() == id)
        {
            m_rules.removeAt(i);
            break;
        }
    }
}
