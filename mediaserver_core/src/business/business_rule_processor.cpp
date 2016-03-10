#include "business_rule_processor.h"

#include <QtCore/QList>
#include <QtCore/QBuffer>
#include <QtGui/QImage>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include <business/business_action_factory.h>
#include <business/business_event_rule.h>
#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/synctime.h>
#include <utils/common/log.h>
#include <utils/common/app_info.h>

#include "business/business_strings_helper.h"
#include "common/common_module.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "core/resource/resource_name.h"

QnBusinessRuleProcessor* QnBusinessRuleProcessor::m_instance = 0;

QnBusinessRuleProcessor::QnBusinessRuleProcessor()
{
    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionDelivered, this, &QnBusinessRuleProcessor::at_actionDelivered);
    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionDeliveryFail, this, &QnBusinessRuleProcessor::at_actionDeliveryFailed);

    connect(qnResPool, &QnResourcePool::resourceAdded,
        this, [this](const QnResourcePtr& resource) { toggleInputPortMonitoring( resource, true ); });
    connect(qnResPool, &QnResourcePool::resourceRemoved,
        this, [this](const QnResourcePtr& resource) { toggleInputPortMonitoring( resource, false ); });

    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionReceived,
        this, static_cast<void (QnBusinessRuleProcessor::*)(const QnAbstractBusinessActionPtr&)>(&QnBusinessRuleProcessor::executeAction));

    connect(QnCommonMessageProcessor::instance(),       &QnCommonMessageProcessor::businessRuleChanged,
            this, &QnBusinessRuleProcessor::at_businessRuleChanged);
    connect(QnCommonMessageProcessor::instance(),       &QnCommonMessageProcessor::businessRuleDeleted,
            this, &QnBusinessRuleProcessor::at_businessRuleDeleted);
    connect(QnCommonMessageProcessor::instance(),       &QnCommonMessageProcessor::businessRuleReset,
            this, &QnBusinessRuleProcessor::at_businessRuleReset);

    connect(&m_timer, &QTimer::timeout, this, &QnBusinessRuleProcessor::at_timer);
    m_timer.start(1000);
    start();
}

QnBusinessRuleProcessor::~QnBusinessRuleProcessor()
{
}

QnMediaServerResourcePtr QnBusinessRuleProcessor::getDestMServer(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    switch(action->actionType())
    {
        case QnBusiness::SendMailAction:
        {
            // looking for server with public IP address
            const QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
            if (!mServer || (mServer->getServerFlags() & Qn::SF_HasPublicIP))
                return QnMediaServerResourcePtr(); // do not proxy

            const auto onlineServers = qnResPool->getAllServers(Qn::Online);
            for (const QnMediaServerResourcePtr& mServer: onlineServers)
            {
                if (mServer->getServerFlags() & Qn::SF_HasPublicIP)
                    return mServer;
            }
            return QnMediaServerResourcePtr();
        }
        case QnBusiness::DiagnosticsAction:
        case QnBusiness::ShowPopupAction:
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        case QnBusiness::SayTextAction:
        case QnBusiness::ShowTextOverlayAction:
        case QnBusiness::ShowOnAlarmLayoutAction:
            return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
        default:
            if (!res)
                return QnMediaServerResourcePtr(); // can not find routeTo resource
            return qnResPool->getResourceById<QnMediaServerResource>(res->getParentId());
    }
}

bool QnBusinessRuleProcessor::needProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    const QnMediaServerResourcePtr routeToServer = getDestMServer(action, res);
    return routeToServer && !action->isReceivedFromRemoteHost() && routeToServer->getId() != getGuid();
}

void QnBusinessRuleProcessor::doProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    const QnMediaServerResourcePtr routeToServer = getDestMServer(action, res);
    if (routeToServer)
    {
        // todo: it is better to use action.clone here
        ec2::ApiBusinessActionData actionData;
        QnAbstractBusinessActionPtr actionToSend;

        ec2::fromResourceToApi(action, actionData);
        if (res) {
            actionData.resourceIds.clear();
            actionData.resourceIds.push_back(res->getId());
        }
        ec2::fromApiToResource(actionData, actionToSend, qnResPool);

        qnBusinessMessageBus->deliveryBusinessAction(actionToSend, routeToServer->getId());
    }
}

void QnBusinessRuleProcessor::executeAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    if (needProxyAction(action, res))
        doProxyAction(action, res);
    else {
        auto actionCopy = QnBusinessActionFactory::cloneAction(action);
        if (res)
            actionCopy->getParams().actionResourceId = res->getId();
        executeActionInternal(actionCopy);
    }
}

void QnBusinessRuleProcessor::executeAction(const QnAbstractBusinessActionPtr& action)
{
    if (!action) {
        Q_ASSERT_X(0, Q_FUNC_INFO, "No action to execute");
        return; // something wrong. It shouldn't be
    }

    QnNetworkResourceList resources = qnResPool->getResources<QnNetworkResource>(action->getResources());

    switch (action->actionType())
    {
    case QnBusiness::ShowTextOverlayAction:
    case QnBusiness::ShowOnAlarmLayoutAction:
        if (action->getParams().useSource)
            resources << qnResPool->getResources<QnNetworkResource>(action->getSourceResources());
        break;
    default:
        break;
    }

    if (resources.isEmpty())
    {
        if (QnBusiness::requiresCameraResource(action->actionType()))
            return; //camera does not exist anymore
        else
            executeAction(action, QnResourcePtr());
    }
    else
    {
        for(const QnResourcePtr& res: resources)
            executeAction(action, res);
    }
}

bool QnBusinessRuleProcessor::executeActionInternal(const QnAbstractBusinessActionPtr& action)
{
    QnResourcePtr res = qnResPool->getResourceById(action->getParams().actionResourceId);
    if (action->isProlonged()) {
        // check for duplicate actions. For example: camera start recording by 2 different events e.t.c
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();

        if (action->getToggleState() == QnBusiness::ActiveState) {
            if (++m_actionInProgress[actionKey] > 1)
                return true; // ignore duplicated start
        } else if (action->getToggleState() == QnBusiness::InactiveState) {
            if (--m_actionInProgress[actionKey] > 0)
                return true; // ignore duplicated stop
            m_actionInProgress.remove(actionKey);
        }
    }

    switch (action->actionType()) {
    case QnBusiness::DiagnosticsAction:
        return true;

    case QnBusiness::ShowPopupAction:
    case QnBusiness::PlaySoundOnceAction:
    case QnBusiness::PlaySoundAction:
    case QnBusiness::SayTextAction:
    case QnBusiness::ShowOnAlarmLayoutAction:
    case QnBusiness::ShowTextOverlayAction:
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

void QnBusinessRuleProcessor::addBusinessRule(const QnBusinessEventRulePtr& value)
{
    at_businessRuleChanged(value);
}

void QnBusinessRuleProcessor::processBusinessEvent(const QnAbstractBusinessEventPtr& bEvent)
{
    QnMutexLocker lock( &m_mutex );

    QnAbstractBusinessActionList actions = matchActions(bEvent);
    for(const QnAbstractBusinessActionPtr& action: actions)
    {
        executeAction(action);
    }
}

bool QnBusinessRuleProcessor::containResource(const QnResourceList& resList, const QnUuid& resId) const
{
    for (int i = 0; i < resList.size(); ++i)
    {
        if (resList.at(i) && resList.at(i)->getId() == resId)
            return true;
    }
    return false;
}

bool QnBusinessRuleProcessor::checkRuleCondition(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule) const
{
    if (!bEvent->isEventStateMatched(rule->eventState(), rule->actionType()))
        return false;
    if (!rule->isScheduleMatchTime(qnSyncTime->currentDateTime()))
        return false;
    return true;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processToggleAction(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    QnAbstractBusinessActionPtr action;

    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];

    if (bEvent->getToggleState() == QnBusiness::InactiveState && !runtimeRule.resources.isEmpty())
        return QnAbstractBusinessActionPtr(); // ignore 'Off' event if some event resources still running

    if (!runtimeRule.isActionRunning.isEmpty())
    {
        // Toggle event repeated with some interval with state 'on'.
        // if toggled action is used and condition is no longer valid - stop action
        // Or toggle event goes to 'off'. stop action
        if (!condOK || bEvent->getToggleState() == QnBusiness::InactiveState)
            action = QnBusinessActionFactory::instantiateAction(rule, bEvent, QnBusiness::InactiveState);
        else
            return QnAbstractBusinessActionPtr(); // ignore repeating 'On' event
    }
    else if (condOK)
        action = QnBusinessActionFactory::instantiateAction(rule, bEvent);

    bool isActionRunning = action && action->getToggleState() == QnBusiness::ActiveState;
    if (isActionRunning)
        runtimeRule.isActionRunning.insert(QnUuid());
    else
        runtimeRule.isActionRunning.clear();

    return action;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processInstantAction(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    RunningRuleMap::iterator itr = m_rulesInProgress.find(rule->getUniqueId());
    if (itr != m_rulesInProgress.end())
    {
        // instant action connected to continue event. update stats to prevent multiple action for repeation 'on' event state
        RunningRuleInfo& runtimeRule = itr.value();
        QnUuid resId = bEvent->getResource() ? bEvent->getResource()->getId() : QnUuid();

        if (condOK && bEvent->getToggleState() == QnBusiness::ActiveState) {
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

    if (rule->eventState() != QnBusiness::UndefinedState && rule->eventState() != bEvent->getToggleState())
        return QnAbstractBusinessActionPtr();


    if (rule->aggregationPeriod() == 0 || !QnBusiness::allowsAggregation(rule->actionType()))
        return QnBusinessActionFactory::instantiateAction(rule, bEvent);

    QString eventKey = rule->getUniqueId();
    if (bEvent->getResource())
        eventKey += bEvent->getResource()->getUniqueId();

    QnProcessorAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    if (!aggInfo.initialized())
        aggInfo.init(bEvent, rule);

    aggInfo.append(bEvent->getRuntimeParams());

    if (aggInfo.isExpired())
    {
        QnAbstractBusinessActionPtr result = QnBusinessActionFactory::instantiateAction(aggInfo.rule(),
                                                                                        aggInfo.event(),
                                                                                        aggInfo.info());
        aggInfo.reset();
        return result;
    }

    return QnAbstractBusinessActionPtr();
}

void QnBusinessRuleProcessor::at_timer()
{
    QnMutexLocker lock( &m_mutex );
    QMap<QString, QnProcessorAggregationInfo>::iterator itr = m_aggregateActions.begin();
    while (itr != m_aggregateActions.end())
    {
        QnProcessorAggregationInfo& aggInfo = itr.value();
        if (aggInfo.totalCount() > 0 && aggInfo.isExpired())
        {
            executeAction(QnBusinessActionFactory::instantiateAction(aggInfo.rule(),
                                                                     aggInfo.event(),
                                                                     aggInfo.info()));
            aggInfo.reset();
        }
        ++itr;
    }
}

bool QnBusinessRuleProcessor::checkEventCondition(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule)
{
    bool resOK = !bEvent->getResource() || rule->eventResources().isEmpty() || rule->eventResources().contains(bEvent->getResource()->getId());
    if (!resOK)
        return false;
    if (!bEvent->checkEventParams(rule->eventParams()))
        return false;


    if (!QnBusiness::hasToggleState(bEvent->getEventType()))
        return true;

    // for continue event put information to m_eventsInProgress
    QnUuid resId = bEvent->getResource() ? bEvent->getResource()->getId() : QnUuid();
    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];
    if (bEvent->getToggleState() == QnBusiness::ActiveState)
        runtimeRule.resources[resId] = bEvent;
    else
        runtimeRule.resources.remove(resId);

    return true;
}

QnAbstractBusinessActionList QnBusinessRuleProcessor::matchActions(const QnAbstractBusinessEventPtr& bEvent)
{
    QnAbstractBusinessActionList result;
    for(const QnBusinessEventRulePtr& rule: m_rules)
    {
        if (rule->isDisabled() || rule->eventType() != bEvent->getEventType())
            continue;
        bool condOK = checkEventCondition(bEvent, rule);
        if (condOK)
        {
            QnAbstractBusinessActionPtr action;

            if (rule->isActionProlonged())
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

void QnBusinessRuleProcessor::at_actionDelivered(const QnAbstractBusinessActionPtr& action)
{
    Q_UNUSED(action)
    //TODO: #vasilenko implement me
}

void QnBusinessRuleProcessor::at_actionDeliveryFailed(const QnAbstractBusinessActionPtr& action)
{
    Q_UNUSED(action)
    //TODO: #vasilenko implement me
}

void QnBusinessRuleProcessor::at_broadcastBusinessActionFinished( int handle, ec2::ErrorCode errorCode )
{
    if (errorCode == ec2::ErrorCode::ok)
        return;

    qWarning() << "error delivering broadcast action message #" << handle << "error:" << ec2::toString(errorCode);
}

bool QnBusinessRuleProcessor::broadcastBusinessAction(const QnAbstractBusinessActionPtr& action)
{
    QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->broadcastBusinessAction(
        action, this, &QnBusinessRuleProcessor::at_broadcastBusinessActionFinished );
    return true;
}

void QnBusinessRuleProcessor::at_businessRuleChanged_i(const QnBusinessEventRulePtr& bRule)
{
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == bRule->id())
        {
            if( !m_rules[i]->isDisabled() )
                notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            if( !bRule->isDisabled() )
                notifyResourcesAboutEventIfNeccessary( bRule, true );
            terminateRunningRule(m_rules[i]);
            m_rules[i] = bRule;
            return;
        }
    }

    //adding new rule
    m_rules << bRule;
    if( !bRule->isDisabled() )
        notifyResourcesAboutEventIfNeccessary( bRule, true );
}

void QnBusinessRuleProcessor::at_businessRuleChanged(const QnBusinessEventRulePtr& bRule)
{
    QnMutexLocker lock( &m_mutex );
    at_businessRuleChanged_i(bRule);
}

void QnBusinessRuleProcessor::at_businessRuleReset(const QnBusinessEventRuleList& rules)
{
    QnMutexLocker lock( &m_mutex );

    // Remove all rules
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if( !m_rules[i]->isDisabled() )
            notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
        terminateRunningRule(m_rules[i]);
    }
    m_rules.clear();

    for(const QnBusinessEventRulePtr& rule: rules) {
        at_businessRuleChanged_i(rule);
    }
}

void QnBusinessRuleProcessor::toggleInputPortMonitoring(const QnResourcePtr& resource, bool toggle)
{
    QnMutexLocker lock( &m_mutex );

    QnVirtualCameraResourcePtr camResource = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camResource)
        return;

    for( const QnBusinessEventRulePtr& rule: m_rules )
    {
        if( rule->isDisabled() )
            continue;

        if( rule->eventType() == QnBusiness::CameraInputEvent)
        {
            QnVirtualCameraResourceList resList = qnResPool->getResources<QnVirtualCameraResource>(rule->eventResources());
            if( resList.isEmpty() ||            //listening all cameras
                resList.contains(camResource) )
            {
                if( toggle )
                    camResource->inputPortListenerAttached();
                else
                    camResource->inputPortListenerDetached();
            }
        }
    }
}

void QnBusinessRuleProcessor::terminateRunningRule(const QnBusinessEventRulePtr& rule)
{
    QString ruleId = rule->getUniqueId();
    RunningRuleInfo runtimeRule = m_rulesInProgress.value(ruleId);
    bool isToggledAction = QnBusiness::hasToggleState(rule->actionType()); // We decided to terminate continues actions only if rule is changed
    if (!runtimeRule.isActionRunning.isEmpty() && !runtimeRule.resources.isEmpty() && isToggledAction)
    {
        for(const QnUuid& resId: runtimeRule.isActionRunning)
        {
            // terminate all actions. If instant action, terminate all resources on which it was started
            QnAbstractBusinessEventPtr bEvent;
            if (!resId.isNull())
                bEvent = runtimeRule.resources.value(resId);
            else
                bEvent = runtimeRule.resources.begin().value(); // for continues action resourceID is not specified and only one record is used
            if (bEvent) {
                QnAbstractBusinessActionPtr action = QnBusinessActionFactory::instantiateAction(rule, bEvent, QnBusiness::InactiveState);
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

void QnBusinessRuleProcessor::at_businessRuleDeleted(QnUuid id)
{
    QnMutexLocker lock( &m_mutex );

    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == id)
        {
            if( !m_rules[i]->isDisabled() )
                notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            terminateRunningRule(m_rules[i]);
            m_rules.removeAt(i);
            break;
        }
    }
}

void QnBusinessRuleProcessor::notifyResourcesAboutEventIfNeccessary( const QnBusinessEventRulePtr& businessRule, bool isRuleAdded )
{
    //notifying resources to start input monitoring
    {
        if( businessRule->eventType() == QnBusiness::CameraInputEvent)
        {
            QnVirtualCameraResourceList resList = qnResPool->getResources<QnVirtualCameraResource>(businessRule->eventResources());
            if (resList.isEmpty())
                resList = qnResPool->getAllCameras(QnResourcePtr(), true);

            for(const QnVirtualCameraResourcePtr &camera: resList)
            {
                if( isRuleAdded )
                    camera->inputPortListenerAttached();
                else
                    camera->inputPortListenerDetached();
            }
        }
    }

    //notifying resources about recording action
    {
        if( businessRule->actionType() == QnBusiness::CameraRecordingAction)
        {
            QnVirtualCameraResourceList resList = qnResPool->getResources<QnVirtualCameraResource>(businessRule->actionResources());
            for(const QnVirtualCameraResourcePtr &camera: resList)
            {
                if( isRuleAdded )
                    camera->recordingEventAttached();
                else
                    camera->recordingEventDetached();
            }
        }
    }
}
