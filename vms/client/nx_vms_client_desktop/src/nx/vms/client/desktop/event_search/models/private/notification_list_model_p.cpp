// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_model_p.h"

#include <analytics/common/object_metadata.h>
#include <business/business_resource_validation.h>
#include <client/client_module.h>
#include <client/client_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/events/license_issue_event.h>
#include <nx/vms/event/events/poe_over_budget_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/time/formatter.h>
#include <ui/common/notification_levels.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/help/business_help.h>
#include <ui/workbench/handlers/workbench_notifications_executor.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/media/audio_player.h>
#include <nx/vms/common/intercom/utils.h>

namespace nx::vms::client::desktop {

using namespace ui;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {

static const auto kDisplayTimeout = std::chrono::milliseconds(12500);

QPixmap toPixmap(const QIcon& icon)
{
    return Skin::maximumSizePixmap(icon);
}

QSharedPointer<AudioPlayer> loopSound(const QString& filePath)
{
    QScopedPointer<AudioPlayer> player(new AudioPlayer());
    if (!player->open(filePath))
        return {};

    const auto restart =
        [filePath, player = player.data()]()
        {
            player->close();
            if (player->open(filePath))
                player->playAsync();
        };

    const auto loopConnection = QObject::connect(player.data(), &AudioPlayer::done,
        player.data(), restart, Qt::QueuedConnection);

    if (!player->playAsync())
        return {};

    return QSharedPointer<AudioPlayer>(player.take(),
        [loopConnection](AudioPlayer* player)
        {
            // Due to AudioPlayer strange architecture simple calling pleaseStop doesn't work well:
            //  it makes the calling thread wait until current playback finishes playing.

            QObject::disconnect(loopConnection);
            if (player->isRunning())
                QObject::connect(player, &AudioPlayer::done, player, &AudioPlayer::deleteLater);
            else
                player->deleteLater();
        });
}

QnResourcePtr getResource(QnUuid resourceId, const QString& cloudSystemId)
{
    if (resourceId.isNull())
        return {};

    SystemContext* systemContext = nullptr;
    if (!cloudSystemId.isEmpty())
        systemContext = appContext()->systemContextByCloudSystemId(cloudSystemId);

    if (!systemContext)
        systemContext = appContext()->currentSystemContext();

    if (!NX_ASSERT(systemContext))
        return {};

    return systemContext->resourcePool()->getResourceById(resourceId);
}

// TODO: #amalov Simplify device transfer logic using list only.
QnVirtualCameraResourceList getActionDevices(
    const nx::vms::rules::NotificationAction* action,
    const QString& cloudSystemId)
{
    auto deviceIds = action->deviceIds();
    if (deviceIds.isEmpty())
        deviceIds << action->cameraId();

    deviceIds.removeAll(QnUuid());

    if (deviceIds.isEmpty())
        return {};

    auto context = cloudSystemId.isEmpty()
        ? nullptr
        : appContext()->cloudCrossSystemManager()->systemContext(cloudSystemId);

    QnVirtualCameraResourceList result;
    for (const auto deviceId : deviceIds)
    {
        auto device = getResource(deviceId, cloudSystemId).dynamicCast<QnVirtualCameraResource>();

        if (!device && context)
        {
            const QString name = deviceIds.count() == 1 ? action->sourceName() : QString();
            device = context->createThumbCameraResource(deviceId, name);
        }

        if (device)
            result << device;
    }

    return result;
}

} // namespace

NotificationListModel::Private::Private(NotificationListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(systemContext()))
{
    const auto handler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, &QnWorkbenchNotificationsHandler::cleared, q, &EventListModel::clear);
    connect(handler, &QnWorkbenchNotificationsHandler::notificationAdded,
        this, &Private::addNotification);
    connect(handler, &QnWorkbenchNotificationsHandler::notificationRemoved,
        this, &Private::removeNotification);

    connect(q, &EventListModel::modelReset, this,
        [this]()
        {
            m_uuidHashes.clear();
            m_itemsByLoadingSound.clear();
            m_players.clear();
            m_itemsByCloudSystem.clear();
        });

    connect(q, &EventListModel::rowsAboutToBeRemoved,
        this, &Private::onRowsAboutToBeRemoved);

    const auto serverNotificationCache = context()->instance<ServerNotificationCache>();
    connect(serverNotificationCache,
        &ServerNotificationCache::fileDownloaded, this,
        [this, serverNotificationCache]
            (const QString& fileName, ServerFileCache::OperationResult status)
        {
            if (status == ServerFileCache::OperationResult::ok)
            {
                const auto path = serverNotificationCache->getFullPath(fileName);
                for (const auto& id: m_itemsByLoadingSound.values(fileName))
                    m_players[id] = loopSound(path);
            }

            m_itemsByLoadingSound.remove(fileName);
        });

    connect(context()->instance<QnWorkbenchNotificationsExecutor>(),
        &QnWorkbenchNotificationsExecutor::notificationActionReceived,
        this,
        [this](const QSharedPointer<nx::vms::rules::NotificationAction>& notificationAction)
        { onNotificationAction(notificationAction); });
    connect(context()->instance<QnWorkbenchNotificationsHandler>(),
        &QnWorkbenchNotificationsHandler::notificationActionReceived,
        this, &NotificationListModel::Private::onNotificationAction);

    auto processNewSystem = [this](const QString& systemId)
    {
        connect(appContext()->cloudCrossSystemManager()->systemContext(systemId),
            &CloudCrossSystemContext::statusChanged, this,
            [this, systemId](CloudCrossSystemContext::Status oldStatus)
            {
                if (oldStatus == CloudCrossSystemContext::Status::connected)
                    removeCloudItems(systemId);
                else
                    updateCloudItems(systemId);
            });
    };

    for (const auto& systemId: appContext()->cloudCrossSystemManager()->cloudSystems())
        processNewSystem(systemId);

    connect(appContext()->cloudCrossSystemManager(), &CloudCrossSystemManager::systemFound,
        this, processNewSystem);

    connect(appContext()->cloudCrossSystemManager(), &CloudCrossSystemManager::systemLost, this,
        [this](const QString& systemId)
        {
            removeCloudItems(systemId);
        });
}

NotificationListModel::Private::~Private()
{
}

void NotificationListModel::Private::updateCloudItems(const QString& systemId)
{
    for (auto it = m_itemsByCloudSystem.find(systemId);
        it != m_itemsByCloudSystem.end() && it.key() == systemId;
        ++it)
    {
        q->setData(q->indexOf(it.value()), false, Qn::ForcePreviewLoaderRole);
    }
}

void NotificationListModel::Private::removeCloudItems(const QString& systemId)
{
    for (auto it = m_itemsByCloudSystem.find(systemId);
        it != m_itemsByCloudSystem.end() && it.key() == systemId;
        ++it)
    {
        q->removeEvent(it.value());
    }

    NX_ASSERT(!m_itemsByCloudSystem.contains(systemId));
}

void NotificationListModel::Private::onRowsAboutToBeRemoved(
    const QModelIndex& parent,
    int first,
    int last)
{
    for (int row = first; row <= last; ++row)
    {
        const auto& event = this->q->getEvent(row);
        m_uuidHashes[event.ruleId][event.source].remove(event.id);
        m_players.remove(event.id);

        for (auto it = m_itemsByLoadingSound.begin(); it != m_itemsByLoadingSound.end(); ++it)
        {
            if (it.value() == event.id)
            {
                m_itemsByLoadingSound.erase(it);
                break;
            }
        }

        if (!event.cloudSystemId.isEmpty() && event.previewCamera)
            m_itemsByCloudSystem.remove(event.cloudSystemId, event.id);
    }
}

void NotificationListModel::Private::onNotificationAction(
    const QSharedPointer<nx::vms::rules::NotificationAction>& action,
    QString cloudSystemId)
{
    NX_VERBOSE(this, "Received action: %1, id: %2, system: %3",
        action->type(), action->id(), cloudSystemId);

    if (!cloudSystemId.isEmpty())
    {
        using Status = CloudCrossSystemContext::Status;

        const auto context = appContext()->cloudCrossSystemManager()->systemContext(cloudSystemId);
        const auto status = context ? context->status() : Status::uninitialized;

        if (status == Status::uninitialized || status == Status::unsupportedPermanently
            || status == Status::unsupportedTemporary)
        {
            NX_VERBOSE(this, "Invalid context status: %1, system: %2", status, cloudSystemId);
            return;
        }
    }

    EventData eventData;
    eventData.id = action->id();
    eventData.title = action->caption();
    eventData.description = action->description();
    eventData.toolTip = action->tooltip();
    eventData.lifetime = kDisplayTimeout;
    eventData.removable = true;
    eventData.timestamp = action->timestamp();
    eventData.level = QnNotificationLevel::convert(action->level());
    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.icon = pixmapForAction(action.get(), cloudSystemId, eventData.titleColor);
    eventData.cloudSystemId = cloudSystemId;
    eventData.ruleId = action->ruleId();

    eventData.objectTrackId = action->objectTrackId();
    eventData.attributes = qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
        action->objectTypeId(),
        action->attributes());

    setupClientAction(action.get(), eventData);

    if (!this->q->addEvent(eventData))
        return;

    if (!cloudSystemId.isEmpty() && eventData.previewCamera)
        m_itemsByCloudSystem.insert(cloudSystemId, eventData.id);

    truncateToMaximumCount();
}

void NotificationListModel::Private::addNotification(const vms::event::AbstractActionPtr& action)
{
    using namespace std::chrono;

    const auto& params = action->getRuntimeParams();
    const auto ruleId = action->getRuleId();

    NX_VERBOSE(this, "Received action: %1, id: %2", action->actionType(), action->getParams().actionId);

    QnResourcePtr resource = resourcePool()->getResourceById(params.eventResourceId);
    auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    const bool hasViewPermission = resource && accessController()->hasPermissions(resource,
        Qn::ViewContentPermission);

    auto title = caption(params, camera);
    const microseconds timestamp(params.eventTimestampUsec);

    auto alarmCameras =
        resourcePool()->getResourcesByIds<QnVirtualCameraResource>(action->getResources());
    if (action->getParams().useSource)
    {
        const auto sourceResources = action->getSourceResources(resourcePool());
        alarmCameras.append(
            action->getSourceResources(resourcePool()).filtered<QnVirtualCameraResource>());
    }
    std::sort(alarmCameras.begin(), alarmCameras.end());
    alarmCameras.erase(std::unique(alarmCameras.begin(), alarmCameras.end()), alarmCameras.end());
    alarmCameras = accessController()->filtered(alarmCameras, Qn::ViewContentPermission);

    const auto actionType = action->actionType();
    if (actionType == ActionType::showOnAlarmLayoutAction)
    {
        if (alarmCameras.isEmpty())
            return;

        const auto iter = m_uuidHashes.find(ruleId);
        if (iter != m_uuidHashes.end())
        {
            for (const auto& ids: *iter)
            {
                for (const auto& id: ids)
                {
                    if (q->indexOf(id).data(Qn::TimestampRole).value<microseconds>() == timestamp)
                        return;
                }
            }
        }

        title = tr("Alarm: %1").arg(title);
    }

    const auto actionId = action->getParams().actionId;
    const bool actionHasId = !actionId.isNull();

    EventData eventData;
    eventData.id = actionHasId ? actionId : QnUuid::createUuid();
    eventData.title = title;
    eventData.description = description(params);
    eventData.toolTip = tooltip(action);
    eventData.helpId = QnBusiness::eventHelpId(params.eventType);
    eventData.level = QnNotificationLevel::valueOf(action);
    eventData.timestamp = timestamp;
    eventData.removable = true;
    eventData.ruleId = action->getRuleId();
    eventData.source = resource;
    eventData.objectTrackId = params.objectTrackId;

    eventData.attributes = qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
        params.getAnalyticsObjectTypeId(), params.attributes);

    if (actionType == ActionType::playSoundAction)
    {
        const auto soundUrl = action->getParams().url;
        if (!m_itemsByLoadingSound.contains(soundUrl))
            context()->instance<ServerNotificationCache>()->downloadFile(soundUrl);

        m_itemsByLoadingSound.insert(soundUrl, eventData.id);
    }
    else if (actionType == ActionType::showOnAlarmLayoutAction)
    {
        eventData.actionId = action::OpenInAlarmLayoutAction;
        eventData.actionParameters = alarmCameras;
        eventData.previewCamera = camera;
        eventData.cameras = alarmCameras;
    }
    else if (actionType == ActionType::showIntercomInformer)
    {
        // Id is fixed and equal to intercom id, so this informer will be removed
        // when another client instance via changing toogle state.
        eventData.id = action->getParams().actionResourceId;
        eventData.actionId = action::OpenIntercomLayoutAction;
        eventData.title = tr("Calling...");
        eventData.description.clear();
        const auto intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(camera);
        eventData.actionParameters = resourcePool()->getResourceById(intercomLayoutId);
        eventData.actionParameters.setArgument(Qn::ActionDataRole, action);
        eventData.previewCamera = camera;
        eventData.removable = false;

        // Recreate informer on the top in case of several calls in a short period.
        q->removeEvent(eventData.id);
    }
    else
    {
        switch (params.eventType)
        {
            case EventType::poeOverBudgetEvent:
            case EventType::fanErrorEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::ServerSettingsAction;
                eventData.actionParameters = action::Parameters(resource);
                eventData.actionParameters.setArgument(Qn::FocusTabRole, QnServerSettingsDialog::PoePage);
                break;
            }

            case EventType::cameraMotionEvent:
            case EventType::softwareTriggerEvent:
            case EventType::analyticsSdkEvent:
            case EventType::analyticsSdkObjectDetected:
            {
                NX_ASSERT(hasViewPermission);
                eventData.cameras = {camera};
                eventData.previewCamera = camera;
                eventData.previewTime = timestamp;
                break;
            }

            case EventType::cameraInputEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.cameras = {camera};
                eventData.previewCamera = camera;
                break;
            }

            case EventType::cameraDisconnectEvent:
            case EventType::networkIssueEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::CameraSettingsAction;
                eventData.actionParameters = camera;
                eventData.previewCamera = camera;
                break;
            }

            case EventType::storageFailureEvent:
            case EventType::backupFinishedEvent:
            case EventType::serverStartEvent:
            case EventType::serverFailureEvent:
            case EventType::serverCertificateError:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::ServerSettingsAction;
                eventData.actionParameters = server;
                break;
            }

            case EventType::cameraIpConflictEvent:
            {
                const auto& webPageAddress = params.caption;
                eventData.actionId = action::BrowseUrlAction;
                eventData.actionParameters = {Qn::UrlRole, webPageAddress};
                break;
            }

            case EventType::licenseIssueEvent:
            {
                eventData.actionId = action::PreferencesLicensesTabAction;
                break;
            }

            case EventType::userDefinedEvent:
            {
                const auto hasPermission =
                    [this](const auto& camera)
                    {
                        return accessController()->hasPermissions(camera,
                            Qn::ViewContentPermission);
                    };

                const auto sourceResources = event::sourceResources(params, resourcePool());

                const QnVirtualCameraResourceList sourceCameras = sourceResources
                    ? sourceResources->filtered<QnVirtualCameraResource>(hasPermission)
                    : QnVirtualCameraResourceList();
                if (!sourceCameras.isEmpty())
                {
                    if (sourceCameras.size() == 1)
                        eventData.previewCamera = sourceCameras[0];
                    eventData.cameras = sourceCameras;
                    eventData.previewTime = timestamp;
                    camera = sourceCameras.first();
                }
                break;
            }

            default:
                break;
        }
    }

    if (eventData.removable && actionType != ActionType::playSoundAction)
        eventData.lifetime = kDisplayTimeout;

    if (actionType == ActionType::showPopupAction && camera)
        setupAcknowledgeAction(eventData, camera->getId(), action);

    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.icon = pixmapForAction(action, eventData.titleColor);

    if (!q->addEvent(eventData))
        return;

    if (!actionHasId)
        m_uuidHashes[action->getRuleId()][resource].insert(eventData.id);

    truncateToMaximumCount();
}

void NotificationListModel::Private::setupClientAction(
    const nx::vms::rules::NotificationAction* action,
    EventData& eventData) const
{
    using nx::vms::rules::ClientAction;

    const auto server = getResource(action->serverId(), eventData.cloudSystemId)
        .dynamicCast<QnMediaServerResource>();

    const auto devices = getActionDevices(action, eventData.cloudSystemId);
    const auto camera = (devices.count() == 1) ? devices.front() : QnVirtualCameraResourcePtr();

    eventData.source = camera ? camera.staticCast<QnResource>() : server.staticCast<QnResource>();
    if (eventData.source || !action->serverId().isNull())
        eventData.sourceName = action->sourceName();

    switch (action->clientAction())
    {
        case ClientAction::none:
            break;

        case ClientAction::poeSettings:
            eventData.actionId = action::ServerSettingsAction;
            eventData.actionParameters = action::Parameters(server);
            eventData.actionParameters
                .setArgument(Qn::FocusTabRole, QnServerSettingsDialog::PoePage);
            break;

        case ClientAction::cameraSettings:
            eventData.actionId = action::CameraSettingsAction;
            eventData.actionParameters = camera;
            eventData.previewCamera = camera;
            break;

        case ClientAction::serverSettings:
            eventData.actionId = action::ServerSettingsAction;
            eventData.actionParameters = server;
            break;

        case ClientAction::licensesSettings:
            eventData.actionId = action::PreferencesLicensesTabAction;
            break;

        case ClientAction::previewCamera:
            eventData.cameras = devices;
            eventData.previewCamera = camera;
            break;

        case ClientAction::previewCameraOnTime:
            eventData.cameras = devices;
            eventData.previewCamera = camera;
            eventData.previewTime = action->timestamp();
            break;

        case ClientAction::browseUrl:
            eventData.actionId = action::BrowseUrlAction;
            eventData.actionParameters = {Qn::UrlRole, action->url()};
            break;

        default:
            NX_ASSERT(false, "Unsupported client action");
            break;
    }
}

void NotificationListModel::Private::removeNotification(const vms::event::AbstractActionPtr& action)
{
    const auto actionId = action->getParams().actionId;
    if (!actionId.isNull())
    {
        q->removeEvent(actionId);
        return;
    }

    if (action->actionType() == ActionType::showIntercomInformer)
    {
        const auto id = action->getParams().actionResourceId;
        if (!id.isNull())
        {
            q->removeEvent(id);
            return;
        }
    }

    const auto ruleId = action->getRuleId();
    const auto iter = m_uuidHashes.find(ruleId);
    if (iter == m_uuidHashes.end())
        return;

    auto& uuidHash = *iter;

    const auto removeEvents =
        [this](const QList<QnUuid>& ids)
        {
            for (const auto& id: ids)
                q->removeEvent(id);
        };

    if (action->actionType() == ActionType::playSoundAction)
    {
        for (const auto& ids: uuidHash.values()) //< Must iterate a copy of the list.
            removeEvents(ids.values());

        return;
    }

    const auto resource = resourcePool()->getResourceById(action->getRuntimeParams().eventResourceId);
    if (!resource)
        return;

    if (uuidHash.contains(resource))
        removeEvents(uuidHash[resource].values());
}

void NotificationListModel::Private::setupAcknowledgeAction(EventData& eventData,
    const QnUuid& cameraId, const nx::vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != vms::api::ActionType::showPopupAction)
    {
        NX_ASSERT(false, "Invalid action type");
        return;
    }

    if (!accessController()->hasGlobalPermission(GlobalPermission::manageBookmarks))
        return;

    auto& actionParams = action->getParams();
    if (!actionParams.requireConfirmation(action->getRuntimeParams().eventType))
        return;

    if (!NX_ASSERT(!cameraId.isNull()))
        return;

    eventData.removable = false;
    eventData.level = QnNotificationLevel::Value::CriticalNotification;

    eventData.extraAction = CommandActionPtr(new CommandAction());
    eventData.extraAction->setIcon(qnSkin->icon("buttons/acknowledge.png"));
    eventData.extraAction->setText(tr("Acknowledge"));

    // TODO: #sivanov Fix hardcoded action parameters.
    static const auto kDurationMs = std::chrono::milliseconds(std::chrono::seconds(10));
    actionParams.durationMs = kDurationMs.count();
    actionParams.recordBeforeMs = 0;

    const auto actionHandler =
        [this, cameraId, action]()
        {
            action::Parameters params;
            const auto camera = resourcePool()->getResourceById(cameraId);
            if (camera && camera->systemContext() && !camera->hasFlags(Qn::removed))
                params.setResources({camera});
            params.setArgument(Qn::ActionDataRole, action);
            menu()->trigger(action::AcknowledgeEventAction, params);
        };

    connect(eventData.extraAction.data(), &QAction::triggered,
        [this, actionHandler]() { executeLater(actionHandler, this); });
}

QString NotificationListModel::Private::caption(const nx::vms::event::EventParameters& parameters,
    const QnVirtualCameraResourcePtr& camera) const
{
    // TODO: #vkutin Include event end condition, if applicable. Include aggregation.
    return m_helper->notificationCaption(parameters, camera);
}

QString NotificationListModel::Private::getPoeOverBudgetDescription(
    const nx::vms::event::EventParameters& parameters) const
{
    static const auto kDescriptionTemplate =
        QString("<font color = '%1'><b>%2</b></font> %3")
        .arg(QPalette().color(QPalette::WindowText).name())
        .arg(m_helper->poeConsumption());

    const auto consumptionString = event::StringsHelper::poeConsumptionStringFromParams(parameters);
    if (consumptionString.isEmpty())
        return QString();

    return kDescriptionTemplate.arg(consumptionString);
}

QString NotificationListModel::Private::description(
    const nx::vms::event::EventParameters& parameters) const
{
    switch (parameters.eventType)
    {
        case EventType::poeOverBudgetEvent:
            return getPoeOverBudgetDescription(parameters);

        case EventType::backupFinishedEvent:
            return m_helper->backupResultText(parameters);

        default:
            break;
    }

    return m_helper->notificationDescription(parameters);
}

QString NotificationListModel::Private::tooltip(const vms::event::AbstractActionPtr& action) const
{
    const auto& params = action->getRuntimeParams();
    const Qn::ResourceInfoLevel resourceInfoLevel = qnSettings->resourceInfoLevel();

    QStringList tooltip = m_helper->eventDescription(
        action,
        vms::event::AggregationInfo(),
        resourceInfoLevel,
        nx::vms::event::AttrSerializePolicy::none);

    // TODO: #sivanov Move this code to ::eventDetails().
    if (params.eventType == EventType::licenseIssueEvent
        && params.reasonCode == vms::api::EventReason::licenseRemoved)
    {
        for (const auto id: nx::vms::event::LicenseIssueEvent::decodeCameras(params))
        {
            NX_ASSERT(!id.isNull());
            if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id))
            {
                if (accessController()->hasPermissions(camera, Qn::ViewContentPermission))
                    tooltip << QnResourceDisplayInfo(camera).toString(resourceInfoLevel);
            }
        }
    }

    return tooltip.join(nx::vms::common::html::kLineBreak);
}

QPixmap NotificationListModel::Private::pixmapForAction(
    const vms::event::AbstractActionPtr& action, const QColor& color) const
{
    if (action->actionType() == ActionType::showIntercomInformer)
        return qnSkin->pixmap("events/call.svg");

    switch (QnNotificationLevel::valueOf(action))
    {
        case QnNotificationLevel::Value::CriticalNotification:
            return qnSkin->pixmap("events/alert_red.png");

        case QnNotificationLevel::Value::ImportantNotification:
            return qnSkin->pixmap("events/alert_yellow.png");

        case QnNotificationLevel::Value::SuccessNotification:
            return qnSkin->pixmap("events/success_mark.png");

        default:
            break;
    }

    if (action->actionType() == ActionType::playSoundAction)
        return qnSkin->pixmap("events/sound.png");

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
        return qnSkin->pixmap("events/alarm.png");

    const auto& params = action->getRuntimeParams();

    if (params.eventType >= EventType::userDefinedEvent)
    {
        const auto sourceResources = event::sourceResources(params, resourcePool());
        if (sourceResources && !sourceResources->isEmpty())
            return toPixmap(qnResIconCache->icon(sourceResources->first()));

         return qnSkin->pixmap("events/alert.png");
    }

    switch (params.eventType)
    {
        case EventType::cameraInputEvent:
        case EventType::cameraIpConflictEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
        {
            const auto resource = resourcePool()->getResourceById(params.eventResourceId);
            return toPixmap(resource
                ? qnResIconCache->icon(resource)
                : qnResIconCache->icon(QnResourceIconCache::Camera));
        }

        case EventType::softwareTriggerEvent:
        {
            return SoftwareTriggerPixmaps::colorizedPixmap(
                action->getRuntimeParams().description,
                color.isValid() ? color : QPalette().light().color());
        }

        case EventType::cameraMotionEvent:
            return qnSkin->pixmap("events/motion.svg");

        case EventType::storageFailureEvent:
            return qnSkin->pixmap("events/storage.png");

        case EventType::cameraDisconnectEvent:
        case EventType::networkIssueEvent:
            return qnSkin->pixmap("events/connection.png");

        case EventType::serverStartEvent:
        case EventType::serverFailureEvent:
        case EventType::serverConflictEvent:
        case EventType::backupFinishedEvent:
        case EventType::serverCertificateError:
            return qnSkin->pixmap("events/server.png");

        case EventType::licenseIssueEvent:
            return qnSkin->pixmap("events/license.png");

        case EventType::pluginDiagnosticEvent:
            return qnSkin->pixmap("events/alert.png");

        default:
            return QPixmap();
    }
}

QPixmap NotificationListModel::Private::pixmapForAction(
    const nx::vms::rules::NotificationAction* action,
    const QString& cloudSystemId,
    const QColor& color) const
{
    using nx::vms::event::Level;
    using nx::vms::rules::Icon;

    static const auto iconFromLevel = QMap<Level, Icon>{
        {Level::critical, Icon::critical},
        {Level::important, Icon::important}
    };

    auto icon = action->icon();
    if (icon == Icon::calculated)
        icon = iconFromLevel.value(action->level(), Icon::none);

    switch (icon)
    {
        case Icon::none:
            return {};

        case Icon::alert:
            return qnSkin->pixmap("events/alert.png");

        case Icon::important:
            return qnSkin->pixmap("events/alert_yellow.png");

        case Icon::critical:
            return qnSkin->pixmap("events/alert_red.png");

        case Icon::server:
            return qnSkin->pixmap("events/server.png");

        case Icon::camera:
            return toPixmap(qnResIconCache->icon(QnResourceIconCache::Camera));

        case Icon::motion:
            return qnSkin->pixmap("events/motion.svg");

        case Icon::resource:
        {
            const auto devices = getActionDevices(action, cloudSystemId);
            return toPixmap(!devices.isEmpty()
                ? qnResIconCache->icon(devices.front())
                : qnResIconCache->icon(QnResourceIconCache::Camera));
        }

        case Icon::custom:
            return SoftwareTriggerPixmaps::colorizedPixmap(
                action->customIcon(),
                color.isValid() ? color : QPalette().light().color());

        default:
            NX_ASSERT(false, "Unhandled icon");
            return {};
    }
}

int NotificationListModel::Private::maximumCount() const
{
    return m_maximumCount;
}

void NotificationListModel::Private::setMaximumCount(int value)
{
    if (m_maximumCount == value)
        return;

    m_maximumCount = value;
    truncateToMaximumCount();
}

void NotificationListModel::Private::truncateToMaximumCount()
{
    const int rowCount = q->rowCount();
    const int countToRemove = rowCount - m_maximumCount;

    if (countToRemove > 0)
        q->removeRows(rowCount - countToRemove, countToRemove);
}

} // namespace nx::vms::client::desktop
