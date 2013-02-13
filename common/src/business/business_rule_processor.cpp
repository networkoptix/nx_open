
#include <QList>
#include "business_rule_processor.h"
#include "core/resource/resource.h"
#include "core/resource/media_server_resource.h"
#include <business/business_event_rule.h>
#include "api/app_server_connection.h"
#include "utils/common/synctime.h"
#include "core/resource_managment/resource_pool.h"

QnBusinessRuleProcessor* QnBusinessRuleProcessor::m_instance = 0;

QnBusinessRuleProcessor::QnBusinessRuleProcessor()
{
    connect(qnBusinessMessageBus, SIGNAL(actionDelivered(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDelivered(QnAbstractBusinessActionPtr)));
    connect(qnBusinessMessageBus, SIGNAL(actionDeliveryFail(QnAbstractBusinessActionPtr)), this, SLOT(at_actionDeliveryFailed(QnAbstractBusinessActionPtr)));

    connect(qnBusinessMessageBus, SIGNAL(actionReceived(QnAbstractBusinessActionPtr, QnResourcePtr)), this, SLOT(executeActionInternal(QnAbstractBusinessActionPtr, QnResourcePtr)));

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
        executeActionInternal(action, QnResourcePtr());
    }
    else {
        for (int i = 0; i < resList.size(); ++i)
        {
            QnMediaServerResourcePtr routeToServer = getDestMServer(action, resList[i]);
            if (routeToServer && !action->isReceivedFromRemoteHost() && routeToServer->getGuid() != getGuid())
                qnBusinessMessageBus->deliveryBusinessAction(action, resList[i], closeDirPath(routeToServer->getApiUrl()) + QLatin1String("api/execAction")); // delivery to other server
            else
                executeActionInternal(action, resList[i]);
        }
    }
}

bool QnBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res)
{
    if (BusinessActionType::hasToggleState(action->actionType()))
    {
        // check for duplicate actions. For example: camera start recording by 2 different events e.t.c
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();

        if (action->getToggleState() == ToggleState::On)
        {
            if (++m_actionInProgress[actionKey] > 1)
                return true; // ignore duplicated start
        }
        else if (action->getToggleState() == ToggleState::Off)
        {
            m_actionInProgress[actionKey] = qMax(0, m_actionInProgress[actionKey]-1);
            if (m_actionInProgress[actionKey] > 0)
                return true; // ignore duplicated stop
        }
    }

    switch( action->actionType() )
    {
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

bool QnBusinessRuleProcessor::checkRuleCondition(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule) const
{
    if (!bEvent->checkCondition(rule->eventState(), rule->eventParams()))
        return false;
    if (!rule->isScheduleMatchTime(qnSyncTime->currentDateTime()))
        return false;
    return true;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    QnAbstractBusinessActionPtr action;

    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];

    if (runtimeRule.isActionRunning)
    {
        // Toggle event repeated with some interval with state 'on'.
        // if toggled action is used and condition is no longer valid - stop action
        // Or toggle event goes to 'off'. stop action
        if (!condOK || bEvent->getToggleState() == ToggleState::Off)
            action = rule->instantiateAction(bEvent, ToggleState::Off); 
        else
            return QnAbstractBusinessActionPtr(); // ignore repeating 'On' event
    }
    else if (condOK)
        action = rule->instantiateAction(bEvent);

    runtimeRule.isActionRunning = action && action->getToggleState() == ToggleState::On;

    return action;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processInstantAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    RunningRuleMap::iterator itr = m_rulesInProgress.find(rule->getUniqueId());
    if (itr != m_rulesInProgress.end())
    {
        // instant action connected to continue event. update stats to prevent multiple action for repeation 'on' event state
        RunningRuleInfo& runtimeRule = itr.value();
        if (condOK && bEvent->getToggleState() == ToggleState::On) {
            if (runtimeRule.isActionRunning)
                return QnAbstractBusinessActionPtr(); // action by rule already was ran. ingore repeated event
            else
                runtimeRule.isActionRunning = true;
        }
        else
            runtimeRule.isActionRunning = false;
    }

    if (!condOK)
        return QnAbstractBusinessActionPtr();

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

    qint64 period = aggInfo.bRule->aggregationPeriod()*1000ll*1000ll;
    if (aggInfo.timeStamp + period < currentTime)
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

bool QnBusinessRuleProcessor::checkEventCondition(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool resOK = !bEvent->getResource() || rule->eventResources().isEmpty() || containResource(rule->eventResources(), bEvent->getResource()->getId());
    if (!resOK)
        return false;


    if (!BusinessEventType::hasToggleState(bEvent->getEventType()))
        return true;
    
    // for continue event put information to m_eventsInProgress
    QnId resId = bEvent->getResource() ? bEvent->getResource()->getId() : QnId();
    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];
	runtimeRule.bEvent = bEvent;
    if (bEvent->getToggleState() == ToggleState::On)
    {
        runtimeRule.resources.insert(resId);
        return true;
    }
    else if (bEvent->getToggleState() == ToggleState::Off) {
        runtimeRule.resources.remove(resId);
        bool isFinished = runtimeRule.resources.isEmpty(); // true if no more running resources by event
        return isFinished;
    }

    return false; // toggle event without state?
}

QList<QnAbstractBusinessActionPtr> QnBusinessRuleProcessor::matchActions(QnAbstractBusinessEventPtr bEvent)
{
    QMutexLocker lock(&m_mutex);
    QList<QnAbstractBusinessActionPtr> result;
    foreach(QnBusinessEventRulePtr rule, m_rules)    
    {
        if (rule->isDisabled() || rule->eventType() != bEvent->getEventType())
            continue;
        bool condOK = checkEventCondition(bEvent, rule);
        if (condOK)
        {
            QnAbstractBusinessActionPtr action;

            if (BusinessActionType::hasToggleState(rule->actionType()))
                action = processToggleAction(bEvent, rule);
            else
                action = processInstantAction(bEvent, rule);

            if (action)
                result << action;

            if (bEvent->getToggleState() == ToggleState::Off)
                m_rulesInProgress.remove(rule->getUniqueId()); // clear running info if event is finished (all running event resources actually finished)
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
    RunningRuleInfo runtimeRule = m_rulesInProgress.value(ruleId);
    if (runtimeRule.isActionRunning) {
        QnAbstractBusinessEventPtr bEvent = runtimeRule.bEvent;
        QnAbstractBusinessActionPtr action = rule->instantiateAction(bEvent, ToggleState::Off); // if toggled action is used and condition is no longer valid - stop action
        if (action)
            executeAction(action);
    }
    m_rulesInProgress.remove(ruleId);

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
        if( businessRule->eventType() == BusinessEventType::BE_Camera_Input)
        {
            QnResourceList resList = businessRule->eventResources();
            if (resList.isEmpty())
                resList = qnResPool->getAllEnabledCameras();

            for( QnResourceList::const_iterator it = resList.constBegin(); it != resList.constEnd(); ++it )
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
