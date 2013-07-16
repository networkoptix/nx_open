#include "business_rule_processor.h"

#include <QtCore/QList>

#include <api/app_server_connection.h>

#include <business/business_action_factory.h>
#include <business/business_event_rule.h>
#include <business/actions/system_health_business_action.h>
#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include "utils/common/synctime.h"
#include <utils/common/email.h>
#include "business_strings_helper.h"

const int EMAIL_SEND_TIMEOUT = 300; // 5 minutes

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
    quit();
    wait();
}

QnMediaServerResourcePtr QnBusinessRuleProcessor::getDestMServer(QnAbstractBusinessActionPtr action, QnResourcePtr res)
{
    if (action->actionType() == BusinessActionType::SendMail || action->actionType() == BusinessActionType::Alert)
        return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
    if (!res)
        return QnMediaServerResourcePtr(); // can not find routeTo resource
    return qnResPool->getResourceById(res->getParentId()).dynamicCast<QnMediaServerResource>();
}

void QnBusinessRuleProcessor::executeAction(QnAbstractBusinessActionPtr action)
{
    QnResourceList resList = action->getResources().filtered<QnNetworkResource>();
    if (resList.isEmpty()) {
        executeActionInternal(action, QnResourcePtr());
    }
    else {
        for (int i = 0; i < resList.size(); ++i)
        {
            QnMediaServerResourcePtr routeToServer = getDestMServer(action, resList[i]);
            if (routeToServer && !action->isReceivedFromRemoteHost() && routeToServer->getGuid() != getGuid())
            {
                // delivery to other server
                QUrl serverUrl = routeToServer->getApiUrl();
                QUrl proxyUrl = QnAppServerConnectionFactory::defaultUrl();
#if 0
                // do proxy via EC builtin proxy. It is dosn't work. I don't know why
                proxyUrl.setPath(QString(QLatin1String("/proxy/http/%1:%2/api/execAction")).arg(serverUrl.host()).arg(serverUrl.port()));
#else
                // do proxy via CPP media proxy
                proxyUrl.setScheme(QLatin1String("http"));
                proxyUrl.setPort(QnAppServerConnectionFactory::defaultMediaProxyPort());
                proxyUrl.setPath(QString(QLatin1String("/proxy/%1:%2/api/execAction")).arg(serverUrl.host()).arg(serverUrl.port()));
#endif

                QString url = proxyUrl.toString();
                qnBusinessMessageBus->deliveryBusinessAction(action, resList[i], url); 
            }
            else {
                executeActionInternal(action, resList[i]);
            }
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

        if (action->getToggleState() == Qn::OnState)
        {
            if (++m_actionInProgress[actionKey] > 1)
                return true; // ignore duplicated start
        }
        else if (action->getToggleState() == Qn::OffState)
        {
            if (--m_actionInProgress[actionKey] > 0)
                return true; // ignore duplicated stop
            m_actionInProgress.remove(actionKey);
        }
    }

    switch( action->actionType() )
    {
    case BusinessActionType::SendMail:
        return sendMail( action.dynamicCast<QnSendMailBusinessAction>() );

    case BusinessActionType::Alert:
        break;

    case BusinessActionType::ShowPopup:
    case BusinessActionType::PlaySound:
    case BusinessActionType::SayText:
        return broadcastBusinessAction(action);

    default:
        break;
    }

    return false;
}

class QnBusinessRuleProcessorInstanceDeleter
{
public:
    ~QnBusinessRuleProcessorInstanceDeleter()
    {
        QnBusinessRuleProcessor::fini();
    }
};

static QnBusinessRuleProcessorInstanceDeleter qnBusinessRuleProcessorInstanceDeleter;

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

void QnBusinessRuleProcessor::fini()
{
    delete m_instance;
    m_instance = NULL;
}

void QnBusinessRuleProcessor::addBusinessRule(QnBusinessEventRulePtr value)
{
    at_businessRuleChanged(value);
}

void QnBusinessRuleProcessor::processBusinessEvent(QnAbstractBusinessEventPtr bEvent)
{
    QMutexLocker lock(&m_mutex);

    QnAbstractBusinessActionList actions = matchActions(bEvent);
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

QString QnBusinessRuleProcessor::formatEmailList(const QStringList &value) {
    QString result;
    for (int i = 0; i < value.size(); ++i)
    {
        if (i > 0)
            result.append(L' ');
        result.append(QString(QLatin1String("%1")).arg(value[i].trimmed()));
    }
    return result;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    QnAbstractBusinessActionPtr action;

    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];

    if (bEvent->getToggleState() == Qn::OffState && !runtimeRule.resources.isEmpty())
        return QnAbstractBusinessActionPtr(); // ignore 'Off' event if some event resources still running

    if (!runtimeRule.isActionRunning.isEmpty())
    {
        // Toggle event repeated with some interval with state 'on'.
        // if toggled action is used and condition is no longer valid - stop action
        // Or toggle event goes to 'off'. stop action
        if (!condOK || bEvent->getToggleState() == Qn::OffState)
            action = QnBusinessActionFactory::instantiateAction(rule, bEvent, Qn::OffState);
        else
            return QnAbstractBusinessActionPtr(); // ignore repeating 'On' event
    }
    else if (condOK)
        action = QnBusinessActionFactory::instantiateAction(rule, bEvent);

    bool isActionRunning = action && action->getToggleState() == Qn::OnState;
    if (isActionRunning)
        runtimeRule.isActionRunning.insert(QnId());
    else
        runtimeRule.isActionRunning.clear();

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
        QnId resId = bEvent->getResource() ? bEvent->getResource()->getId() : QnId();

        if (condOK && bEvent->getToggleState() == Qn::OnState) {
            if (runtimeRule.isActionRunning.contains(resId))
                return QnAbstractBusinessActionPtr(); // action by rule already was ran. Allow separate action for each resource of source event but ingore repeated 'on' state
            else
                runtimeRule.isActionRunning.insert(resId);
        }
        else
            runtimeRule.isActionRunning.remove(resId);
    }

    if (!condOK)
        return QnAbstractBusinessActionPtr();

    if (rule->eventState() != Qn::UndefinedState && rule->eventState() != bEvent->getToggleState())
        return QnAbstractBusinessActionPtr();


    if (rule->aggregationPeriod() == 0)
        return QnBusinessActionFactory::instantiateAction(rule, bEvent);

    QString eventKey = rule->getUniqueId();
    if (bEvent->getResource())
        eventKey += bEvent->getResource()->getUniqueId();


    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();

    QnProcessorAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    if (!aggInfo.initialized())
        aggInfo.init(bEvent, rule, currentTime);

    aggInfo.append(bEvent->getRuntimeParams());

    if (currentTime > aggInfo.estimatedEnd())
    {
        QnAbstractBusinessActionPtr result = QnBusinessActionFactory::instantiateAction(aggInfo.rule(),
                                                                                        aggInfo.event(),
                                                                                        aggInfo.info());
        aggInfo.reset(currentTime);
        return result;
    }

    return QnAbstractBusinessActionPtr();
}

void QnBusinessRuleProcessor::at_timer()
{
    QMutexLocker lock(&m_mutex);
    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    QMap<QString, QnProcessorAggregationInfo>::iterator itr = m_aggregateActions.begin();
    while (itr != m_aggregateActions.end())
    {
        QnProcessorAggregationInfo& aggInfo = itr.value();
        if (aggInfo.totalCount() > 0 && currentTime > aggInfo.estimatedEnd())
        {
            executeAction(QnBusinessActionFactory::instantiateAction(aggInfo.rule(),
                                                                     aggInfo.event(),
                                                                     aggInfo.info()));
            aggInfo.reset(currentTime);
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
    if (bEvent->getToggleState() == Qn::OnState)
        runtimeRule.resources[resId] = bEvent;
    else 
        runtimeRule.resources.remove(resId);

    return true;
}

QnAbstractBusinessActionList QnBusinessRuleProcessor::matchActions(QnAbstractBusinessEventPtr bEvent)
{
    QnAbstractBusinessActionList result;
    foreach(QnBusinessEventRulePtr rule, m_rules)    
    {
        if (rule->disabled() || rule->eventType() != bEvent->getEventType())
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

            RunningRuleMap::Iterator itr = m_rulesInProgress.find(rule->getUniqueId());
            if (itr != m_rulesInProgress.end() && itr->resources.isEmpty())
                m_rulesInProgress.erase(itr); // clear running info if event is finished (all running event resources actually finished)
        }
    }
    return result;
}

void QnBusinessRuleProcessor::at_actionDelivered(QnAbstractBusinessActionPtr action)
{
    Q_UNUSED(action)
    //TODO: #vasilenko implement me
}

void QnBusinessRuleProcessor::at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action)
{
    Q_UNUSED(action)
    //TODO: #vasilenko implement me
}

bool QnBusinessRuleProcessor::sendMail(const QnSendMailBusinessActionPtr& action )
{
    Q_ASSERT( action );

    QStringList log;
    QStringList recipients;
    foreach (const QnUserResourcePtr &user, action->getResources().filtered<QnUserResource>()) {
        QString email = user->getEmail();
        log << QString(QLatin1String("%1 <%2>")).arg(user->getName()).arg(user->getEmail());
        if (!email.isEmpty() && QnEmail::isValid(email))
            recipients << email;
    }

    QStringList additional = action->getParams().getEmailAddress().split(QLatin1Char(';'), QString::SkipEmptyParts);
    foreach(const QString &email, additional) {
        log << email;
        QString trimmed = email.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (QnEmail::isValid(trimmed))
            recipients << email;
    }

    if( recipients.isEmpty() )
    {
        cl_log.log( QString::fromLatin1("Action SendMail (rule %1) missing valid recipients. Ignoring...").arg(action->getBusinessRuleId()), cl_logWARNING );
        cl_log.log( QString::fromLatin1("All recipients: ") + log.join(QLatin1String("; ")), cl_logWARNING );
        return false;
    }

    cl_log.log( QString::fromLatin1("Processing action SendMail. Sending mail to %1").
        arg(recipients.join(QLatin1String("; "))), cl_logDEBUG1 );


    const QnAppServerConnectionPtr& appServerConnection = QnAppServerConnectionFactory::createConnection();    
    appServerConnection->sendEmailAsync(
                recipients,
                QnBusinessStringsHelper::eventAtResource(action->getRuntimeParams(), true),
                QnBusinessStringsHelper::eventDescription(action, action->aggregationInfo(), true, true),
                EMAIL_SEND_TIMEOUT,
                this,
                SLOT(at_sendEmailFinished(int,bool,int)));

    /*
     * This action instance is not used anymore but storing into the Events Log db.
     * Therefore we are storing all used emails in order to not recalculate them in
     * the event log processing methods. --rvasilenko
     */
    action->getParams().setEmailAddress(formatEmailList(recipients));
    return true;
}

void QnBusinessRuleProcessor::at_sendEmailFinished(int status, bool result, int handle)
{
    Q_UNUSED(status)
    Q_UNUSED(handle)
    if (result)
        return;

    QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::EmailSendError));

    broadcastBusinessAction(action);

    cl_log.log(QString::fromLatin1("Error processing action SendMail."), cl_logWARNING);
}

void QnBusinessRuleProcessor::at_broadcastBusinessActionFinished(QnHTTPRawResponse response, int handle)
{
    if (response.status == 0)
        return;

    qWarning() << "error delivering broadcast action message #" << handle << "error:" << response.errorString;
}

bool QnBusinessRuleProcessor::broadcastBusinessAction(QnAbstractBusinessActionPtr action)
{
    const QnAppServerConnectionPtr& appServerConnection = QnAppServerConnectionFactory::createConnection();
    appServerConnection->broadcastBusinessAction(action, this, SLOT(at_broadcastBusinessActionFinished(QnHTTPRawResponse, int)));
    return true;
}

void QnBusinessRuleProcessor::at_businessRuleChanged(QnBusinessEventRulePtr bRule)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == bRule->id())
        {
            if( !m_rules[i]->disabled() )
                notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            if( !bRule->disabled() )
                notifyResourcesAboutEventIfNeccessary( bRule, true );
            terminateRunningRule(m_rules[i]);
            m_rules[i] = bRule;
            return;
        }
    }

    //adding new rule
    m_rules << bRule;
    if( !bRule->disabled() )
        notifyResourcesAboutEventIfNeccessary( bRule, true );
}

void QnBusinessRuleProcessor::terminateRunningRule(QnBusinessEventRulePtr rule)
{
    QString ruleId = rule->getUniqueId();
    RunningRuleInfo runtimeRule = m_rulesInProgress.value(ruleId);
    bool isToggledAction = BusinessActionType::hasToggleState(rule->actionType()); // We decided to terminate continues actions only if rule is changed
    if (!runtimeRule.isActionRunning.isEmpty() && !runtimeRule.resources.isEmpty() && isToggledAction)
    {
        foreach(const QnId& resId, runtimeRule.isActionRunning)
        {
            // terminate all actions. If instant action, terminate all resources on which it was started
            QnAbstractBusinessEventPtr bEvent;
            if (resId.isValid())
                bEvent = runtimeRule.resources.value(resId);
            else
                bEvent = runtimeRule.resources.begin().value(); // for continues action resourceID is not specified and only one record is used
            if (bEvent) {
                QnAbstractBusinessActionPtr action = QnBusinessActionFactory::instantiateAction(rule, bEvent, Qn::OffState);
                if (action)
                    executeAction(action);
            }
        }
    }
    m_rulesInProgress.remove(ruleId);

    QString aggKey = rule->getUniqueId();
    QMap<QString, QnProcessorAggregationInfo>::iterator itr = m_aggregateActions.lowerBound(aggKey);
    while (itr != m_aggregateActions.end())
    {
        itr = m_aggregateActions.erase(itr);
        if (itr == m_aggregateActions.end() || !itr.key().startsWith(aggKey))
            break;
    }
}

void QnBusinessRuleProcessor::at_businessRuleDeleted(int id)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == id)
        {
            if( !m_rules[i]->disabled() )
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
        if( businessRule->eventType() == BusinessEventType::Camera_Input)
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
        if( businessRule->actionType() == BusinessActionType::CameraRecording)
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
