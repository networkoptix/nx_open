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
#include <nx/utils/log/log.h>
#include <utils/common/app_info.h>

#include <business/business_strings_helper.h>
#include <business/event_rule_manager.h>
#include <common/common_module.h>
#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <database/server_db.h>
#include <common/common_module.h>

namespace {

QnUuid getActionRunningKey(const QnAbstractBusinessActionPtr& action)
{
    const auto key = action->getExternalUniqKey();
    return guidFromArbitraryData(key);
}

} // namespace

QnBusinessRuleProcessor::QnBusinessRuleProcessor(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionDelivered, this, &QnBusinessRuleProcessor::at_actionDelivered);
    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionDeliveryFail, this, &QnBusinessRuleProcessor::at_actionDeliveryFailed);

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, [this](const QnResourcePtr& resource) { toggleInputPortMonitoring( resource, true ); },
		Qt::QueuedConnection);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, [this](const QnResourcePtr& resource) { toggleInputPortMonitoring( resource, false ); },
		Qt::QueuedConnection);

    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionReceived,
        this, static_cast<void (QnBusinessRuleProcessor::*)(const QnAbstractBusinessActionPtr&)>(&QnBusinessRuleProcessor::executeAction),
		Qt::QueuedConnection);

    connect(eventRuleManager(), &QnEventRuleManager::ruleAddedOrUpdated,
            this, &QnBusinessRuleProcessor::at_eventRuleChanged);
    connect(eventRuleManager(), &QnEventRuleManager::ruleRemoved,
            this, &QnBusinessRuleProcessor::at_eventRuleRemoved);
    connect(eventRuleManager(), &QnEventRuleManager::rulesReset,
            this, &QnBusinessRuleProcessor::at_eventRulesReset);

    connect(&m_timer, &QTimer::timeout, this, &QnBusinessRuleProcessor::at_timer, Qt::QueuedConnection);
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
            const QnMediaServerResourcePtr mServer = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
            if (!mServer || (mServer->getServerFlags() & Qn::SF_HasPublicIP))
                return QnMediaServerResourcePtr(); // do not proxy

            const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
            for (const QnMediaServerResourcePtr& mServer: onlineServers)
            {
                if (mServer->getServerFlags() & Qn::SF_HasPublicIP)
                    return mServer;
            }
            return QnMediaServerResourcePtr();
        }
        case QnBusiness::DiagnosticsAction:
        case QnBusiness::ShowPopupAction:
        case QnBusiness::ShowTextOverlayAction:
        case QnBusiness::ShowOnAlarmLayoutAction:
        case QnBusiness::ExecHttpRequestAction:
            return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
        default:
            if (!res)
                return QnMediaServerResourcePtr(); // can not find routeTo resource
            return resourcePool()->getResourceById<QnMediaServerResource>(res->getParentId());
    }
}

bool QnBusinessRuleProcessor::needProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    if (action->isReceivedFromRemoteHost())
        return false;

    if (action->isProlonged())
    {
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();
        if (m_actionInProgress.contains(actionKey))
            return false; //< do not proxy until finish locally started action
    }

    const QnMediaServerResourcePtr routeToServer = getDestMServer(action, res);
    return routeToServer && routeToServer->getId() != getGuid();
}

void QnBusinessRuleProcessor::doProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    if (action->isProlonged())
    {
        // remove started actions because it actions for other server
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();
        m_actionInProgress.remove(actionKey);
    }

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
        ec2::fromApiToResource(actionData, actionToSend);

        qnBusinessMessageBus->deliveryBusinessAction(actionToSend, routeToServer->getId());
        // we need to save action to the log before proxy. It need for event log for 'view video' operation.
        // Otherwise foreign server can't perform this
        qnServerDb->saveActionToDB(action);
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

bool QnBusinessRuleProcessor::updateProlongedActionStartTime(
    const QnAbstractBusinessActionPtr& action)
{
    if (!action)
    {
        NX_EXPECT(false, "Action is null");
        return false;
    }

    if (action->getParams().durationMs > 0
        || action->getToggleState() != QnBusiness::ActiveState)
    {
        return false;
    }

    const auto key = getActionRunningKey(action);
    const auto startTimeUsec = action->getRuntimeParams().eventTimestampUsec;
    m_runningBookmarkActions[key] = startTimeUsec;
    return true;
}

bool QnBusinessRuleProcessor::popProlongedActionStartTime(
    const QnAbstractBusinessActionPtr& action,
    qint64& startTimeUsec)
{
    if (!action)
    {
        NX_EXPECT(false, "Invalid action");
        return false;
    }
    if (action->getParams().durationMs > 0
        || action->getToggleState() == QnBusiness::ActiveState)
    {
        return false;
    }

    const auto key = getActionRunningKey(action);
    const auto it = m_runningBookmarkActions.find(key);
    if (it == m_runningBookmarkActions.end())
    {
        NX_EXPECT(false, "Can't find prolonged action data");
        return false;
    }

    startTimeUsec = *it;
    m_runningBookmarkActions.erase(it);
    return true;
}

bool QnBusinessRuleProcessor::fixActionTimeFields(const QnAbstractBusinessActionPtr& action)
{
    if (!action)
    {
        NX_EXPECT(false, "Invalid action");
        return false;
    }

    const bool isProlonged = action->getParams().durationMs <= 0;
    if (isProlonged && updateProlongedActionStartTime(action))
        return false; //< Do not process event until it is finished

    qint64 startTimeUsec = action->getRuntimeParams().eventTimestampUsec;
    if (isProlonged && !popProlongedActionStartTime(action, startTimeUsec))
    {
        NX_EXPECT(false, "Something went wrong");
        return false; //< Do not process event at all
    }

    if (isProlonged)
    {
        const auto endTimeUsec = action->getRuntimeParams().eventTimestampUsec;
        action->getParams().durationMs = (endTimeUsec - startTimeUsec) / 1000;
        action->getRuntimeParams().eventTimestampUsec = startTimeUsec;
    }

    return true;
}

bool QnBusinessRuleProcessor::handleBookmarkAction(const QnAbstractBusinessActionPtr& action)
{
    if (!action->getParams().needConfirmation)
        return false;

    if (action->actionType() != QnBusiness::BookmarkAction)
    {
        NX_EXPECT(false, "Invalid action");
        return false;
    }

    if (!fixActionTimeFields(action))
        return false;

    action->getParams().targetActionType = action->actionType();
    action->setActionType(QnBusiness::ShowPopupAction);

    broadcastBusinessAction(action);
    return true;
}

void QnBusinessRuleProcessor::executeAction(const QnAbstractBusinessActionPtr& action)
{
    if (!action) {
        NX_ASSERT(0, Q_FUNC_INFO, "No action to execute");
        return; // something wrong. It shouldn't be
    }
    prepareAdditionActionParams(action);

    QnNetworkResourceList resources = resourcePool()->getResources<QnNetworkResource>(action->getResources());

    switch (action->actionType())
    {
        case QnBusiness::ShowTextOverlayAction:
        case QnBusiness::ShowOnAlarmLayoutAction:
            if (action->getParams().useSource)
                resources << resourcePool()->getResources<QnNetworkResource>(action->getSourceResources());
            break;

        case QnBusiness::SayTextAction:
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
            {
                // execute say to client once and before proxy
                if(!action->isReceivedFromRemoteHost() && action->getParams().playToClient)
                {
                    broadcastBusinessAction(action);
                    // This actions marked as requiredCameraResource, but can be performed to client without camRes
                    if (resources.isEmpty())
                        qnServerDb->saveActionToDB(action);
                }
                break;
            }

        case QnBusiness::BookmarkAction:
        {
            if (handleBookmarkAction(action))
                return;

            break;
        }
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
    auto bRuleId = action->getBusinessRuleId();
    QnResourcePtr res = resourcePool()->getResourceById(action->getParams().actionResourceId);
    if (action->isProlonged()) {
        // check for duplicate actions. For example: camera start recording by 2 different events e.t.c
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();

        if (action->getToggleState() == QnBusiness::ActiveState)
        {
            QSet<QnUuid>& runningRules = m_actionInProgress[actionKey];
            runningRules.insert(bRuleId);
            if (runningRules.size() > 2)
                return true; // ignore duplicated start
        } else if (action->getToggleState() == QnBusiness::InactiveState)
        {
            QSet<QnUuid>& runningRules = m_actionInProgress[actionKey];
            runningRules.remove(bRuleId);
            if (!runningRules.isEmpty())
                return true; // ignore duplicated stop
            m_actionInProgress.remove(actionKey);
        }
    }

    switch (action->actionType()) {
    case QnBusiness::DiagnosticsAction:
        return true;

    case QnBusiness::ShowPopupAction:
    case QnBusiness::ShowOnAlarmLayoutAction:
    case QnBusiness::ShowTextOverlayAction:
        return broadcastBusinessAction(action);

    default:
        break;
    }

    return false;
}

void QnBusinessRuleProcessor::addBusinessRule(const QnBusinessEventRulePtr& value)
{
    at_eventRuleChanged(value);
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
            action = QnBusinessActionFactory::instantiateAction(
                rule,
                bEvent,
                commonModule()->moduleGUID(),
                QnBusiness::InactiveState);
        else
            return QnAbstractBusinessActionPtr(); // ignore repeating 'On' event
    }
    else if (condOK)
    {
        action = QnBusinessActionFactory::instantiateAction(
            rule,
            bEvent,
            commonModule()->moduleGUID());
    }

    bool isActionRunning = action && action->getToggleState() == QnBusiness::ActiveState;
    if (isActionRunning)
        runtimeRule.isActionRunning.insert(QnUuid());
    else
        runtimeRule.isActionRunning.clear();

    return action;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processInstantAction(
    const QnAbstractBusinessEventPtr& bEvent,
    const QnBusinessEventRulePtr& rule)
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
        return QnBusinessActionFactory::instantiateAction(rule, bEvent, commonModule()->moduleGUID());

    QString eventKey = rule->getUniqueId();
    if (bEvent->getResource() && bEvent->getEventType() != QnBusiness::SoftwareTriggerEvent)
        eventKey += bEvent->getResource()->getUniqueId();

    QnProcessorAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    if (!aggInfo.initialized())
        aggInfo.init(bEvent, rule);

    aggInfo.append(bEvent->getRuntimeParams());

    if (aggInfo.isExpired())
    {
        QnAbstractBusinessActionPtr result = QnBusinessActionFactory::instantiateAction(
            aggInfo.rule(),
            aggInfo.event(),
            commonModule()->moduleGUID(),
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
            executeAction(QnBusinessActionFactory::instantiateAction(
                aggInfo.rule(),
                aggInfo.event(),
                commonModule()->moduleGUID(),
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
    commonModule()->ec2Connection()->getBusinessEventManager(Qn::kSystemAccess)->broadcastBusinessAction(
        action, this, &QnBusinessRuleProcessor::at_broadcastBusinessActionFinished );
    return true;
}

void QnBusinessRuleProcessor::at_eventRuleChanged_i(const QnBusinessEventRulePtr& bRule)
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

void QnBusinessRuleProcessor::at_eventRuleChanged(const QnBusinessEventRulePtr& bRule)
{
    QnMutexLocker lock(&m_mutex);
    at_eventRuleChanged_i(bRule);
}

void QnBusinessRuleProcessor::at_eventRulesReset(const QnBusinessEventRuleList& rules)
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
        at_eventRuleChanged_i(rule);
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
            QnVirtualCameraResourceList resList = resourcePool()->getResources<QnVirtualCameraResource>(rule->eventResources());
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
            if (bEvent)
            {
                QnAbstractBusinessActionPtr action = QnBusinessActionFactory::instantiateAction(
                    rule,
                    bEvent,
                    commonModule()->moduleGUID(),
                    QnBusiness::InactiveState);
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

void QnBusinessRuleProcessor::at_eventRuleRemoved(QnUuid id)
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
            QnVirtualCameraResourceList resList = resourcePool()->getResources<QnVirtualCameraResource>(businessRule->eventResources());
            if (resList.isEmpty())
                resList = resourcePool()->getAllCameras(QnResourcePtr(), true);

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
            QnVirtualCameraResourceList resList = resourcePool()->getResources<QnVirtualCameraResource>(businessRule->actionResources());
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
