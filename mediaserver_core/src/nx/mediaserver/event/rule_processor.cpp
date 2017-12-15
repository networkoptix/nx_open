#include "rule_processor.h"

#include <functional>

#include <QtCore/QList>
#include <QtCore/QBuffer>
#include <QtGui/QImage>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <database/server_db.h>
#include <utils/common/synctime.h>
#include <utils/common/app_info.h>
#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/utils/log/log.h>
#include <nx/vms/event/action_factory.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/actions/send_mail_action.h>
#include <nx/vms/event/actions/camera_output_action.h>

namespace {

QnUuid getActionRunningKey(const nx::vms::event::AbstractActionPtr& action)
{
    const auto key = action->getExternalUniqKey()
        + action->getParams().actionResourceId.toString();
    return guidFromArbitraryData(key);
}

} // namespace

namespace nx {
namespace mediaserver {
namespace event {

RuleProcessor::RuleProcessor(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(qnEventMessageBus, &EventMessageBus::actionDelivered,
        this, &RuleProcessor::at_actionDelivered);
    connect(qnEventMessageBus, &EventMessageBus::actionDeliveryFail, this,
        &RuleProcessor::at_actionDeliveryFailed);
    connect(qnEventMessageBus, &EventMessageBus::actionReceived,
        this, &RuleProcessor::executeAction, Qt::QueuedConnection);

    using namespace std::placeholders;

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, std::bind(&RuleProcessor::toggleInputPortMonitoring, this, _1, true),
    	Qt::QueuedConnection);

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, std::bind(&RuleProcessor::toggleInputPortMonitoring, this, _1, false),
        Qt::QueuedConnection);

    connect(eventRuleManager(), &vms::event::RuleManager::ruleAddedOrUpdated,
            this, &RuleProcessor::at_ruleAddedOrUpdated);
    connect(eventRuleManager(), &vms::event::RuleManager::ruleRemoved,
            this, &RuleProcessor::at_ruleRemoved);
    connect(eventRuleManager(), &vms::event::RuleManager::rulesReset,
            this, &RuleProcessor::at_rulesReset);

    connect(&m_timer, &QTimer::timeout, this, &RuleProcessor::at_timer, Qt::QueuedConnection);
    m_timer.start(1000);
    start();
}

RuleProcessor::~RuleProcessor()
{
}

QnMediaServerResourcePtr RuleProcessor::getDestinationServer(
    const vms::event::AbstractActionPtr& action, const QnResourcePtr& res)
{
    switch(action->actionType())
    {
        case vms::event::sendMailAction:
        {
            // Look for server with public IP address.
            const auto server = resourcePool()->getResourceById<QnMediaServerResource>(
                commonModule()->moduleGUID());
            if (!server || (server->getServerFlags() & Qn::SF_HasPublicIP))
                return QnMediaServerResourcePtr(); //< Do not proxy.

            const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
            for (const auto& server: onlineServers)
            {
                if (server->getServerFlags() & Qn::SF_HasPublicIP)
                    return server;
            }
            return QnMediaServerResourcePtr();
        }

        case vms::event::diagnosticsAction:
        case vms::event::showPopupAction:
        case vms::event::showTextOverlayAction:
        case vms::event::showOnAlarmLayoutAction:
        case vms::event::execHttpRequestAction:
            return QnMediaServerResourcePtr(); //< Don't transfer to other server, execute here.

        default:
            return res
                ? resourcePool()->getResourceById<QnMediaServerResource>(res->getParentId())
                : QnMediaServerResourcePtr(); //< Can't find route to resource.
    }
}

bool RuleProcessor::needProxyAction(const vms::event::AbstractActionPtr& action,
    const QnResourcePtr& res)
{
    if (action->isReceivedFromRemoteHost())
        return false;

    if (action->isProlonged())
    {
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();
        if (m_actionInProgress.contains(actionKey))
            return false; //< Don't proxy until locally started action finishes.
    }

    const auto routeToServer = getDestinationServer(action, res);
    return routeToServer && routeToServer->getId() != getGuid();
}

void RuleProcessor::doProxyAction(const vms::event::AbstractActionPtr& action,
    const QnResourcePtr& res)
{
    if (action->isProlonged())
    {
        // Remove started action because it moves to another server.
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();
        m_actionInProgress.remove(actionKey);
    }

    const auto routeToServer = getDestinationServer(action, res);
    if (routeToServer)
    {
        // TODO: #rvasilenko It is better to use action.clone() here.
        ec2::ApiBusinessActionData actionData;
        vms::event::AbstractActionPtr actionToSend;

        ec2::fromResourceToApi(action, actionData);
        if (res)
        {
            actionData.resourceIds.clear();
            actionData.resourceIds.push_back(res->getId());
        }

        ec2::fromApiToResource(actionData, actionToSend);
        qnEventMessageBus->deliverAction(actionToSend, routeToServer->getId());

        // We need to save action to the log before proxy.
        // It is needed for event log for 'view video' operation.
        // Foreign server won't be able to perform this.
        qnServerDb->saveActionToDB(action);
    }
}

void RuleProcessor::doExecuteAction(const vms::event::AbstractActionPtr& action,
    const QnResourcePtr& res)
{
    if (needProxyAction(action, res))
    {
        doProxyAction(action, res);
    }
    else
    {
        auto actionCopy = vms::event::ActionFactory::cloneAction(action);
        if (res)
            actionCopy->getParams().actionResourceId = res->getId();

        executeActionInternal(actionCopy);
    }
}

void RuleProcessor::executeAction(const vms::event::AbstractActionPtr& action)
{
    if (!action)
    {
        NX_ASSERT(0, Q_FUNC_INFO, "No action to execute");
        return; //< Something is wrong.
    }

    prepareAdditionActionParams(action);

    auto resources = resourcePool()->getResources<QnNetworkResource>(action->getResources());

    switch (action->actionType())
    {
        case vms::event::showTextOverlayAction:
        case vms::event::showOnAlarmLayoutAction:
        {
            if (action->getParams().useSource)
                resources << resourcePool()->getResources<QnNetworkResource>(action->getSourceResources());
            break;
        }

        case vms::event::sayTextAction:
        case vms::event::playSoundAction:
        case vms::event::playSoundOnceAction:
        {
            if (!action->isReceivedFromRemoteHost() && action->getParams().playToClient)
            {
                broadcastAction(action);
                // These actions are marked as requiring camera resource,
                // but in "play to client" mode they don't require it.
                if (resources.isEmpty())
                    qnServerDb->saveActionToDB(action);
            }
            break;
        }

        default:
            break;
    }

    if (resources.isEmpty())
    {
        if (vms::event::requiresCameraResource(action->actionType()))
            return; //< Camera doesn't exist anymore.
        else
            doExecuteAction(action, QnResourcePtr());
    }
    else
    {
        for (const auto& res: resources)
            doExecuteAction(action, res);
    }
}

bool RuleProcessor::executeActionInternal(const vms::event::AbstractActionPtr& action)
{
    auto ruleId = action->getRuleId();
    auto res = resourcePool()->getResourceById(action->getParams().actionResourceId);

    if (action->isProlonged())
    {
        // Check for duplicate actions.
        // For example, camera starts recording by 2 different events etc.
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();

        if (action->getToggleState() == vms::event::EventState::active)
        {
            QSet<QnUuid>& runningRules = m_actionInProgress[actionKey];
            runningRules.insert(ruleId);
            if (runningRules.size() > 2)
                return true; //< Ignore duplicated start.
        }
        else if (action->getToggleState() == vms::event::EventState::inactive)
        {
            QSet<QnUuid>& runningRules = m_actionInProgress[actionKey];
            runningRules.remove(ruleId);
            if (!runningRules.isEmpty())
                return true; //< Ignore duplicated stop.

            m_actionInProgress.remove(actionKey);
        }
    }

    switch (action->actionType())
    {
        case vms::event::diagnosticsAction:
            return true;

        case vms::event::showPopupAction:
            action->getParams().actionId = QnUuid::createUuid();
            /*fallthrough*/
        case vms::event::showOnAlarmLayoutAction:
        case vms::event::showTextOverlayAction:
            return broadcastAction(action);
        default:
            break;
    }

    return false;
}

bool RuleProcessor::updateProlongedActionStartTime(const vms::event::AbstractActionPtr& action)
{
    if (!action)
    {
        NX_EXPECT(false, "Action is null");
        return false;
    }

    if (action->getParams().durationMs > 0
        || action->getToggleState() != vms::event::EventState::active)
    {
        return false;
    }

    const auto key = getActionRunningKey(action);
    const auto startTimeUsec = action->getRuntimeParams().eventTimestampUsec;
    m_runningBookmarkActions[key] = startTimeUsec;
    return true;
}

bool RuleProcessor::popProlongedActionStartTime(
    const vms::event::AbstractActionPtr& action,
    qint64& startTimeUsec)
{
    if (!action)
    {
        NX_EXPECT(false, "Invalid action");
        return false;
    }

    if (action->getParams().durationMs > 0
        || action->getToggleState() == vms::event::EventState::active)
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

bool RuleProcessor::fixActionTimeFields(const vms::event::AbstractActionPtr& action)
{
    if (!action)
    {
        NX_EXPECT(false, "Invalid action");
        return false;
    }

    const bool isProlonged = action->getParams().durationMs <= 0;
    if (isProlonged && updateProlongedActionStartTime(action))
        return false; //< Do not process event until it's finished.

    qint64 startTimeUsec = action->getRuntimeParams().eventTimestampUsec;
    if (isProlonged && !popProlongedActionStartTime(action, startTimeUsec))
    {
        NX_EXPECT(false, "Something went wrong");
        return false; //< Do not process event at all.
    }

    if (isProlonged)
    {
        const auto endTimeUsec = action->getRuntimeParams().eventTimestampUsec;
        action->getParams().durationMs = (endTimeUsec - startTimeUsec) / 1000;
        action->getRuntimeParams().eventTimestampUsec = startTimeUsec;
    }

    return true;
}
void RuleProcessor::addRule(const vms::event::RulePtr& value)
{
    at_ruleAddedOrUpdated(value);
}

void RuleProcessor::processEvent(const vms::event::AbstractEventPtr& event)
{
    QnMutexLocker lock(&m_mutex);

    const auto actions = matchActions(event);

    for (const auto& action: actions)
        executeAction(action);
}

bool RuleProcessor::containsResource(const QnResourceList& resList, const QnUuid& resId) const
{
    for (const auto& resource: resList)
    {
        if (resource && resource->getId() == resId)
            return true;
    }
    return false;
}

bool RuleProcessor::checkRuleCondition(const vms::event::AbstractEventPtr& event,
    const vms::event::RulePtr& rule) const
{
    if (!event->isEventStateMatched(rule->eventState(), rule->actionType()))
        return false;
    if (!rule->isScheduleMatchTime(qnSyncTime->currentDateTime()))
        return false;
    return true;
}

vms::event::AbstractActionPtr RuleProcessor::processToggleableAction(
    const vms::event::AbstractEventPtr& event, const vms::event::RulePtr& rule)
{
    const bool condOK = checkRuleCondition(event, rule);
    vms::event::AbstractActionPtr action;

    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];

    // Ignore 'Off' event if some event resources are still running.
    if (event->getToggleState() == vms::event::EventState::inactive && !runtimeRule.resources.isEmpty())
        return vms::event::AbstractActionPtr();

    if (!runtimeRule.isActionRunning.isEmpty())
    {
        // Toggle event repeated with some interval with state 'on'.
        // If toggled action is used and condition is no longer valid, stop action.
        // If toggle event goes to 'off', stop action.
        if (!condOK || event->getToggleState() == vms::event::EventState::inactive)
            action = vms::event::ActionFactory::instantiateAction(
                rule,
                event,
                commonModule()->moduleGUID(),
                vms::event::EventState::inactive);
        else
            return vms::event::AbstractActionPtr(); //< Ignore repeating 'On' event.
    }
    else if (condOK)
    {
        action = vms::event::ActionFactory::instantiateAction(
            rule,
            event,
            commonModule()->moduleGUID());
    }

    bool isActionRunning = action && action->getToggleState() == vms::event::EventState::active;
    if (isActionRunning)
        runtimeRule.isActionRunning.insert(QnUuid());
    else
        runtimeRule.isActionRunning.clear();

    return action;
}

vms::event::AbstractActionPtr RuleProcessor::processInstantAction(
    const vms::event::AbstractEventPtr& event,
    const vms::event::RulePtr& rule)
{
    bool condOK = checkRuleCondition(event, rule);
    RunningRuleMap::iterator itr = m_rulesInProgress.find(rule->getUniqueId());
    if (itr != m_rulesInProgress.end())
    {
        // Instant action connected to continue event.
        // Update stats to prevent multiple action for repeation 'on' event state.
        RunningRuleInfo& runtimeRule = itr.value();
        QnUuid resId = event->getResource() ? event->getResource()->getId() : QnUuid();

        if (condOK && event->getToggleState() == vms::event::EventState::active)
        {
            // Allow separate action for each resource of source event but ingore repeated 'on' state.
            if (runtimeRule.isActionRunning.contains(resId))
                return vms::event::AbstractActionPtr(); //< If action by rule was already run.
            else
                runtimeRule.isActionRunning.insert(resId);
        }
        else
        {
            runtimeRule.isActionRunning.remove(resId);
        }
    }

    if (!condOK)
        return vms::event::AbstractActionPtr();

    if (rule->eventState() != vms::event::EventState::undefined && rule->eventState() != event->getToggleState())
        return vms::event::AbstractActionPtr();


    if (rule->aggregationPeriod() == 0 || !vms::event::allowsAggregation(rule->actionType()))
        return vms::event::ActionFactory::instantiateAction(rule, event, commonModule()->moduleGUID());

    QString eventKey = rule->getUniqueId();
    if (event->getResource() && event->getEventType() != vms::event::softwareTriggerEvent)
        eventKey += event->getResource()->getUniqueId();

    ProcessorAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    if (!aggInfo.initialized())
        aggInfo.init(event, rule);

    aggInfo.append(event->getRuntimeParams());

    if (aggInfo.isExpired())
    {
        vms::event::AbstractActionPtr result = vms::event::ActionFactory::instantiateAction(
            aggInfo.rule(),
            aggInfo.event(),
            commonModule()->moduleGUID(),
            aggInfo.info());
        aggInfo.reset();
        return result;
    }

    return vms::event::AbstractActionPtr();
}

void RuleProcessor::at_timer()
{
    QnMutexLocker lock(&m_mutex);
    QMap<QString, ProcessorAggregationInfo>::iterator itr = m_aggregateActions.begin();
    while (itr != m_aggregateActions.end())
    {
        auto& aggInfo = itr.value();
        if (aggInfo.totalCount() > 0 && aggInfo.isExpired())
        {
            executeAction(vms::event::ActionFactory::instantiateAction(
                aggInfo.rule(),
                aggInfo.event(),
                commonModule()->moduleGUID(),
                aggInfo.info()));
            aggInfo.reset();
        }
        ++itr;
    }
}

bool RuleProcessor::checkEventCondition(const vms::event::AbstractEventPtr& event,
    const vms::event::RulePtr& rule)
{
    const bool resOK = !event->getResource()
        || rule->eventResources().isEmpty()
        || rule->eventResources().contains(event->getResource()->getId());

    if (!resOK)
        return false;

    if (!event->checkEventParams(rule->eventParams()))
        return false;

    if (!vms::event::hasToggleState(event->getEventType()))
        return true;

    // For continuing event put information to m_eventsInProgress.
    QnUuid resId = event->getResource() ? event->getResource()->getId() : QnUuid();
    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];
    if (event->getToggleState() == vms::event::EventState::active)
        runtimeRule.resources[resId] = event;
    else
        runtimeRule.resources.remove(resId);

    return true;
}

vms::event::AbstractActionList RuleProcessor::matchActions(
    const vms::event::AbstractEventPtr& event)
{
    vms::event::AbstractActionList result;
    for (const vms::event::RulePtr& rule: m_rules)
    {
        if (rule->isDisabled() || rule->eventType() != event->getEventType())
            continue;
        bool condOK = checkEventCondition(event, rule);
        if (condOK)
        {
            vms::event::AbstractActionPtr action;

            if (rule->isActionProlonged())
                action = processToggleableAction(event, rule);
            else
                action = processInstantAction(event, rule);

            if (action)
                result << action;

            RunningRuleMap::Iterator itr = m_rulesInProgress.find(rule->getUniqueId());
            if (itr != m_rulesInProgress.end() && itr->resources.isEmpty())
                m_rulesInProgress.erase(itr); // clear running info if event is finished (all running event resources actually finished)
        }
    }
    return result;
}

void RuleProcessor::at_actionDelivered(const vms::event::AbstractActionPtr& action)
{
    Q_UNUSED(action)
    // TODO: #vasilenko implement me.
}

void RuleProcessor::at_actionDeliveryFailed(const vms::event::AbstractActionPtr& action)
{
    Q_UNUSED(action)
    // TODO: #vasilenko implement me.
}

void RuleProcessor::at_broadcastActionFinished(int handle, ec2::ErrorCode errorCode)
{
    if (errorCode == ec2::ErrorCode::ok)
        return;

    qWarning() << "error delivering broadcast action message #" << handle << "error:" << ec2::toString(errorCode);
}

bool RuleProcessor::broadcastAction(const vms::event::AbstractActionPtr& action)
{
    commonModule()->ec2Connection()->getBusinessEventManager(Qn::kSystemAccess)->broadcastBusinessAction(
        action, this, &RuleProcessor::at_broadcastActionFinished);
    return true;
}

void RuleProcessor::at_ruleAddedOrUpdated_impl(const vms::event::RulePtr& rule)
{
    for (auto& other: m_rules)
    {
        if (other->id() == rule->id())
        {
            if (!other->isDisabled())
                notifyResourcesAboutEventIfNeccessary(other, false);
            if (!rule->isDisabled())
                notifyResourcesAboutEventIfNeccessary(rule, true);
            terminateRunningRule(other);
            other = rule;
            return;
        }
    }

    // Add new rule.
    m_rules << rule;
    if (!rule->isDisabled())
        notifyResourcesAboutEventIfNeccessary(rule, true);
}

void RuleProcessor::at_ruleAddedOrUpdated(const vms::event::RulePtr& rule)
{
    QnMutexLocker lock(&m_mutex);
    at_ruleAddedOrUpdated_impl(rule);
}

void RuleProcessor::at_rulesReset(const vms::event::RuleList& rules)
{
    QnMutexLocker lock(&m_mutex);

    // Remove all rules.
    for (const auto& rule: m_rules)
    {
        if (!rule->isDisabled())
            notifyResourcesAboutEventIfNeccessary(rule, false);
        terminateRunningRule(rule);
    }
    m_rules.clear();

    for (const auto& rule: rules)
        at_ruleAddedOrUpdated_impl(rule);
}

void RuleProcessor::toggleInputPortMonitoring(const QnResourcePtr& resource, bool on)
{
    QnMutexLocker lock(&m_mutex);

    auto camResource = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camResource)
        return;

    for (const auto& rule: m_rules)
    {
        if (rule->isDisabled())
            continue;

        if (rule->eventType() == vms::event::cameraInputEvent)
        {
            auto resList = resourcePool()->getResources<QnVirtualCameraResource>(rule->eventResources());
            if (resList.isEmpty() ||            //< Listening to all cameras.
                resList.contains(camResource))
            {
                if (on)
                    camResource->inputPortListenerAttached();
                else
                    camResource->inputPortListenerDetached();
            }
        }
    }
}

void RuleProcessor::terminateRunningRule(const vms::event::RulePtr& rule)
{
    QString ruleId = rule->getUniqueId();
    RunningRuleInfo runtimeRule = m_rulesInProgress.value(ruleId);
    bool isToggledAction = vms::event::hasToggleState(rule->actionType()); //< We decided to terminate continuous actions only if rule is changed.
    if (!runtimeRule.isActionRunning.isEmpty() && !runtimeRule.resources.isEmpty() && isToggledAction)
    {
        for (const QnUuid& resId: runtimeRule.isActionRunning)
        {
            // Terminate all actions. If action is instant, terminate all resources on which it was started.
            vms::event::AbstractEventPtr event;
            if (!resId.isNull())
                event = runtimeRule.resources.value(resId);
            else
                event = runtimeRule.resources.begin().value(); //< For continuous action, resourceID is not specified and only one record is used.
            if (event)
            {
                vms::event::AbstractActionPtr action = vms::event::ActionFactory::instantiateAction(
                    rule,
                    event,
                    commonModule()->moduleGUID(),
                    vms::event::EventState::inactive);
                if (action)
                    executeAction(action);
            }
        }
    }
    m_rulesInProgress.remove(ruleId);

    QString aggKey = rule->getUniqueId();
    QMap<QString, ProcessorAggregationInfo>::iterator itr = m_aggregateActions.lowerBound(aggKey);
    while (itr != m_aggregateActions.end())
    {
        itr = m_aggregateActions.erase(itr);
        if (itr == m_aggregateActions.end() || !itr.key().startsWith(aggKey))
            break;
    }
}

void RuleProcessor::at_ruleRemoved(QnUuid id)
{
    QnMutexLocker lock(&m_mutex);

    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == id)
        {
            if (!m_rules[i]->isDisabled())
                notifyResourcesAboutEventIfNeccessary(m_rules[i], false);
            terminateRunningRule(m_rules[i]);
            m_rules.removeAt(i);
            break;
        }
    }
}

void RuleProcessor::notifyResourcesAboutEventIfNeccessary(
    const vms::event::RulePtr& businessRule, bool isRuleAdded)
{
    // Notifying resources to start input monitoring.
    {
        if (businessRule->eventType() == vms::event::cameraInputEvent)
        {
            auto resList = resourcePool()->getResources<QnVirtualCameraResource>(
                businessRule->eventResources());
            if (resList.isEmpty())
                resList = resourcePool()->getAllCameras(QnResourcePtr(), true);

            for (const auto& camera: resList)
            {
                if (isRuleAdded)
                    camera->inputPortListenerAttached();
                else
                    camera->inputPortListenerDetached();
            }
        }
    }

    // Notifying resources about recording action.
    {
        if (businessRule->actionType() == vms::event::cameraRecordingAction)
        {
            auto resList = resourcePool()->getResources<QnVirtualCameraResource>(
                businessRule->actionResources());
            for (const auto& camera: resList)
            {
                if (isRuleAdded)
                    camera->recordingEventAttached();
                else
                    camera->recordingEventDetached();
            }
        }
    }
}

} // namespace event
} // namespace mediaserver
} // namespace nx
