
#include <QList>
#include "business_rule_processor.h"
#include "core/resource/resource.h"
#include "core/resource/media_server_resource.h"
#include "events/business_event_rule.h"
#include "api/app_server_connection.h"
#include "utils/common/synctime.h"
#include "core/resource_managment/resource_pool.h"

QnBusinessRuleProcessor* QnBusinessRuleProcessor::m_instance = 0;

QnBusinessRuleProcessor::QnBusinessRuleProcessor()
{
    connect(qnBusinessMessageBus, SIGNAL(actionDelivered(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDelivered(QnAbstractBusinessActionPtr)));
    connect(qnBusinessMessageBus, SIGNAL(actionDeliveryFail(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDeliveryFailed(QnAbstractBusinessActionPtr)));

    connect(qnBusinessMessageBus, SIGNAL(actionReceived(QnAbstractBusinessActionPtr)), this, SLOT(executeAction(QnAbstractBusinessActionPtr)));

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(at_timer()));
    m_timer.start(1000);
    start();
}

QnBusinessRuleProcessor::~QnBusinessRuleProcessor()
{
}

QnMediaServerResourcePtr QnBusinessRuleProcessor::getDestMServer(QnAbstractBusinessActionPtr action, QnResourcePtr res)
{
    if (action->actionType() == BusinessActionType::BA_SendMail || action->actionType() == BusinessActionType::BA_Alert)
        return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
    if (!res)
        return QnMediaServerResourcePtr(); // can not find routeTo resource
    return qnResPool->getResourceById(res->getParentId()).dynamicCast<QnMediaServerResource>();
}

void QnBusinessRuleProcessor::executeAction(QnAbstractBusinessActionPtr action)
{
    QnResourceList resList = action->getResources();
    if (resList.isEmpty()) {
        executeActionInternal(action);
    }
    else {
        for (int i = 0; i < resList.size(); ++i)
        {
            QnMediaServerResourcePtr routeToServer = getDestMServer(action, resList[i]);
            if (routeToServer && !action->isReceivedFromRemoteHost() /*&& routeToServer->getGuid() != getGuid()*/)
                qnBusinessMessageBus->deliveryBusinessAction(action, closeDirPath(routeToServer->getApiUrl()) + QLatin1String("api/execAction")); // delivery to other server
            else
                executeActionInternal(action);
        }
    }
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

    notifyResourcesAboutEventIfNeccessary( value, true );
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
        if (resList.at(i) && resList.at(i)->getId() == resId)
            return true;
    }
    return false;
}

bool QnBusinessRuleProcessor::checkCondition(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule) const
{
    if (rule->isDisabled())
        return false;
    if (!bEvent->checkCondition(rule->eventState(), rule->eventParams()))
        return false;
    if (!rule->isScheduleMatchTime(qnSyncTime->currentDateTime()))
        return false;
    return true;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = checkCondition(bEvent, rule);
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
        m_rulesInProgress.insert(rule->getUniqueId(), bEvent);
    else
        m_rulesInProgress.remove(rule->getUniqueId());

    return action;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processInstantAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = checkCondition(bEvent, rule);
    if (!condOK)
        return QnAbstractBusinessActionPtr();
    
    if (bEvent->getToggleState() == ToggleState::On) {
        if (m_rulesInProgress.contains(rule->getUniqueId()))
            return QnAbstractBusinessActionPtr(); // rule already in progress. ingore repeated event
        else
            m_rulesInProgress.insert(rule->getUniqueId(), bEvent);
    }
    else {
        m_rulesInProgress.remove(rule->getUniqueId());
    }

    if (rule->eventState() != ToggleState::NotDefined && rule->eventState() != bEvent->getToggleState())
        return QnAbstractBusinessActionPtr();


    if (rule->aggregationPeriod() == 0)
        return rule->instantiateAction(bEvent);

    QString eventKey = rule->getUniqueId();
    if (bEvent->getResource())
        eventKey += bEvent->getResource()->getUniqueId();

    QAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    aggInfo.count++;
    aggInfo.bEvent = bEvent;
    aggInfo.bRule = rule;

    if (aggInfo.timeStamp + rule->aggregationPeriod() < currentTime)
    {
        QnAbstractBusinessActionPtr action = aggInfo.bRule->instantiateAction(aggInfo.bEvent);
        action->setAggregationCount(aggInfo.count);
        executeAction(action);
        aggInfo.count = 0;
        aggInfo.timeStamp = currentTime;
    }

    return QnAbstractBusinessActionPtr();
}

void QnBusinessRuleProcessor::at_timer()
{
    QMutexLocker lock(&m_mutex);
    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    QMap<QString, QAggregationInfo>::iterator itr = m_aggregateActions.begin();
    while (itr != m_aggregateActions.end())
    {
        QAggregationInfo& aggInfo = itr.value();
        qint64 period = aggInfo.bRule->aggregationPeriod()*1000ll*1000ll;
        if (aggInfo.count > 0 && aggInfo.timeStamp + period < currentTime)
        {
            QnAbstractBusinessActionPtr action = aggInfo.bRule->instantiateAction(aggInfo.bEvent);
            action->setAggregationCount(aggInfo.count);
            executeAction(action);
            aggInfo.count = 0;
            aggInfo.timeStamp = currentTime;
        }
        ++itr;
    }
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

bool QnBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action )
{
    bool rez = true;
    foreach(const QnResourcePtr& resource, action->getResources())
        rez &= triggerCameraOutput(action, resource);
    return rez;
}

bool QnBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, QnResourcePtr resource )
{
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
            emailAddress.split(QLatin1Char(';'), QString::SkipEmptyParts),
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
            notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            notifyResourcesAboutEventIfNeccessary( bRule, true );
            terminateRunningRule(m_rules[i]);
            m_rules[i] = bRule;
            return;
        }
    }
    m_rules << bRule;
    notifyResourcesAboutEventIfNeccessary( bRule, true );
}

void QnBusinessRuleProcessor::terminateRunningRule(QnBusinessEventRulePtr rule)
{
    QString ruleId = rule->getUniqueId();
    if (m_rulesInProgress.contains(ruleId)) {
        QnAbstractBusinessEventPtr bEvent = m_rulesInProgress[ruleId];
        QnAbstractBusinessActionPtr action = rule->instantiateAction(bEvent, ToggleState::Off); // if toggled action is used and condition is no longer valid - stop action
        if (action)
            executeAction(action);
        m_rulesInProgress.remove(ruleId);
    }

    QString aggKey = rule->getUniqueId();
    QMap<QString, QAggregationInfo>::iterator itr = m_aggregateActions.lowerBound(aggKey);
    while (itr != m_aggregateActions.end())
    {
        itr = m_aggregateActions.erase(itr);
        if (itr == m_aggregateActions.end() || !itr.key().startsWith(aggKey))
            break;
    }
}

void QnBusinessRuleProcessor::at_businessRuleDeleted(QnId id)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->getId() == id)
        {
            notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            terminateRunningRule(m_rules[i]);
            m_rules.removeAt(i);
            break;
        }
    }
}

void QnBusinessRuleProcessor::notifyResourcesAboutEventIfNeccessary( QnBusinessEventRulePtr businessRule, bool isRuleAdded )
{
    //notifying resources to start input monitoring
    {
        const QnResourceList& resList = businessRule->eventResources();
        if( businessRule->eventType() == BusinessEventType::BE_Camera_Input)
        {
            for( QnResourceList::const_iterator it = resList.begin(); it != resList.end(); ++it )
            {
                QnSharedResourcePointer<QnSecurityCamResource> securityCam = it->dynamicCast<QnSecurityCamResource>();
                if( !securityCam )
                    continue;
                if( isRuleAdded )
                    securityCam->inputPortListenerAttached();
                else
                    securityCam->inputPortListenerDetached();
            }
        }
    }

    //notifying resources about recording action
    {
        const QnResourceList& resList = businessRule->actionResources();
        if( businessRule->actionType() == BusinessActionType::BA_CameraRecording)
        {
            for( QnResourceList::const_iterator it = resList.begin(); it != resList.end(); ++it )
            {
                QnSharedResourcePointer<QnSecurityCamResource> securityCam = it->dynamicCast<QnSecurityCamResource>();
                if( !securityCam )
                    continue;
                if( isRuleAdded )
                    securityCam->recordingEventAttached();
                else
                    securityCam->recordingEventDetached();
            }
        }
    }
}
