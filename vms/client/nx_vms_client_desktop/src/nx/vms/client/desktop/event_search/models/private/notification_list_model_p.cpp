// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_model_p.h"

#include <analytics/common/object_metadata.h>
#include <business/business_resource_validation.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/event_search/utils/event_data.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_executor.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/events/license_issue_event.h>
#include <nx/vms/event/events/poe_over_budget_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/utils/type.h>
#include <nx/vms/time/formatter.h>
#include <ui/common/notification_levels.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/media/audio_player.h>

namespace nx::vms::client::desktop {

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {

static const auto kDisplayTimeout = std::chrono::milliseconds(12500);

static const QColor kLight12Color = "#91A7B2";
static const QColor kLight10Color = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QnIcon::Normal, {{kLight12Color, "light12"}, {kLight10Color, "light10"}}},
};

QPixmap toPixmap(const QIcon& icon)
{
    return core::Skin::maximumSizePixmap(icon);
}

QSharedPointer<AudioPlayer> loopSound(const QString& filePath)
{
    auto player = std::make_unique<AudioPlayer>();
    if (!player->open(filePath))
        return {};

    const auto restart =
        [filePath, player = player.get()]()
        {
            player->close();
            if (player->open(filePath))
                player->playAsync();
        };

    const auto loopConnection = QObject::connect(player.get(), &AudioPlayer::done,
        player.get(), restart, Qt::QueuedConnection);

    if (!player->playAsync())
        return {};

    return QSharedPointer<AudioPlayer>(player.release(),
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

QnVirtualCameraResourceList getAlarmCameras(
    const nx::vms::event::AbstractAction* action,
    const nx::vms::client::desktop::SystemContext* context)
{
    const auto resourcePool = context->resourcePool();
    auto alarmCameras = resourcePool->getResourcesByIds<QnVirtualCameraResource>(
        action->getResources());

    if (action->getParams().useSource)
    {
        const auto sourceResourceIds = action->getSourceResources(resourcePool);
        alarmCameras.append(resourcePool->getResourcesByIds<QnVirtualCameraResource>(
            sourceResourceIds));
    }

    std::sort(alarmCameras.begin(), alarmCameras.end());
    alarmCameras.erase(std::unique(alarmCameras.begin(), alarmCameras.end()), alarmCameras.end());

    alarmCameras = alarmCameras.filtered(
        [context](const auto& camera)
        {
            return context->accessController()->hasPermissions(camera, Qn::ViewContentPermission);
        });

    return alarmCameras;
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

void fillEventData(
    const nx::vms::rules::NotificationActionBase* action,
    EventListModel::EventData& eventData)
{
    eventData.id = action->id();
    eventData.ruleId = action->ruleId();
    eventData.title = action->caption();
    eventData.description = action->description();
    eventData.toolTip = action->tooltip();
    eventData.removable = true;
    eventData.timestamp = action->timestamp();
    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);

    eventData.objectTrackId = action->objectTrackId();
    eventData.attributes = qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
        action->objectTypeId(),
        action->attributes());
}

} // namespace

NotificationListModel::Private::Private(NotificationListModel* q):
    WindowContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(system()))
{
    const auto handler = windowContext()->notificationActionHandler();
    connect(handler, &NotificationActionHandler::cleared, q, &EventListModel::clear);
    connect(handler, &NotificationActionHandler::notificationAdded,
        this, &Private::addNotification);
    connect(handler, &NotificationActionHandler::notificationRemoved,
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

    const auto serverNotificationCache = workbenchContext()->instance<ServerNotificationCache>();
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

    connect(workbenchContext()->instance<NotificationActionExecutor>(),
        &NotificationActionExecutor::notificationActionReceived,
        this, &NotificationListModel::Private::onNotificationActionBase);

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
    const auto eventIds = m_itemsByCloudSystem.values(systemId);
    int removedCount = 0;

    for (const auto id: eventIds)
    {
        if (q->removeEvent(id))
            ++removedCount;
    }

    NX_ASSERT(!m_itemsByCloudSystem.contains(systemId),
        "System: %1, events: %2, removed %3", systemId, eventIds.size(), removedCount);
}

void NotificationListModel::Private::onRowsAboutToBeRemoved(
    const QModelIndex& parent,
    int first,
    int last)
{
    for (int row = first; row <= last; ++row)
    {
        const auto& event = this->q->getEvent(row);
        m_uuidHashes[event.ruleId][event.sourceId()].remove(event.id);
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

void NotificationListModel::Private::onNotificationActionBase(
    const QSharedPointer<nx::vms::rules::NotificationActionBase>& action,
    const QString& cloudSystemId)
{
    using namespace nx::vms::rules;

    const auto& actionType = action->type();

    NX_VERBOSE(this, "Received action: %1, state: %2, id: %3",
        actionType, action->state(), action->id());

    if (actionType == vms::rules::utils::type<NotificationAction>())
        onNotificationAction(action.dynamicCast<NotificationAction>(), cloudSystemId);
    else if (actionType == vms::rules::utils::type<RepeatSoundAction>())
        onRepeatSoundAction(action.dynamicCast<RepeatSoundAction>());
    else if (actionType == vms::rules::utils::type<ShowOnAlarmLayoutAction>())
        onAlarmLayoutAction(action.dynamicCast<ShowOnAlarmLayoutAction>());
    else
        NX_ASSERT(false, "Unexpected action type: %1", actionType);
}

void NotificationListModel::Private::onNotificationAction(
    const QSharedPointer<nx::vms::rules::NotificationAction>& action,
    const QString& cloudSystemId)
{
    NX_VERBOSE(this, "Cloud system id: %1", cloudSystemId);

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
    eventData.lifetime = kDisplayTimeout;
    eventData.level = QnNotificationLevel::convert(action->level());
    eventData.icon = pixmapForAction(action.get(), cloudSystemId, eventData.titleColor);
    eventData.cloudSystemId = cloudSystemId;
    fillEventData(action.get(), eventData);

    setupClientAction(action.get(), eventData);

    if (!this->q->addEvent(eventData))
        return;

    if (!cloudSystemId.isEmpty() && eventData.previewCamera)
        m_itemsByCloudSystem.insert(cloudSystemId, eventData.id);

    truncateToMaximumCount();
}

void NotificationListModel::Private::onRepeatSoundAction(
    const QSharedPointer<nx::vms::rules::RepeatSoundAction>& action)
{
    if (action->state() == nx::vms::rules::State::started)
    {
        EventData eventData;
        eventData.level = QnNotificationLevel::convert(nx::vms::event::Level::common);
        eventData.icon =
            qnSkin->icon("events/sound_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));
        eventData.sourceName = action->sourceName();
        fillEventData(action.get(), eventData);

        if (!this->q->addEvent(eventData))
            return;

        const auto soundUrl = action->sound();
        if (!m_itemsByLoadingSound.contains(soundUrl))
            workbenchContext()->instance<ServerNotificationCache>()->downloadFile(soundUrl);

        m_itemsByLoadingSound.insert(soundUrl, eventData.id);

        m_uuidHashes[action->ruleId()][eventData.sourceId()].insert(eventData.id);

        truncateToMaximumCount();
    }
    else if (action->state() == nx::vms::rules::State::stopped)
    {
        removeAllItems(action->ruleId());
    }
    else
    {
        NX_ASSERT(this, "Unexpected action state: %1", action->state());
    }
}

void NotificationListModel::Private::onAlarmLayoutAction(
    const QSharedPointer<nx::vms::rules::ShowOnAlarmLayoutAction>& action)
{
    NX_ASSERT(action->state() != nx::vms::rules::State::stopped,
        "Unexpected alarm action state: %1", action->state());

    EventData eventData;
    eventData.lifetime = kDisplayTimeout;
    eventData.level = QnNotificationLevel::convert(nx::vms::event::Level::critical);
    eventData.icon = qnSkin->icon("events/alarm_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));
    eventData.sourceName = action->sourceName();
    eventData.previewCamera =
        system()->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(action->eventDeviceIds())
            .value(0);
    eventData.cameras =
        system()->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(action->deviceIds());
    eventData.actionId = menu::OpenInAlarmLayoutAction;
    eventData.actionParameters = eventData.cameras;

    fillEventData(action.get(), eventData);

    if (!this->q->addEvent(eventData))
        return;

    // TODO: #amalov Check the need of using m_uuidHashes for this action.
    truncateToMaximumCount();
}

void NotificationListModel::Private::addNotification(const vms::event::AbstractActionPtr& action)
{
    using namespace std::chrono;

    const auto& params = action->getRuntimeParams();
    const auto ruleId = action->getRuleId();
    const auto actionId = action->getParams().actionId;
    // The actionId is set for notifications only, see RuleProcessor::executeActionInternal.
    // Used for hiding acknowledged notifications.
    const bool actionHasId = !actionId.isNull();

    const auto actionType = action->actionType();

    NX_VERBOSE(this, "Received action: %1, id: %2", actionType, actionId);

    QnResourcePtr resource = system()->resourcePool()->getResourceById(params.eventResourceId);
    auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto server = resource.dynamicCast<QnMediaServerResource>();

    auto title = caption(params, camera);
    const microseconds timestamp(params.eventTimestampUsec);

    auto alarmCameras = getAlarmCameras(action.get(), system());

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

    EventData eventData;
    eventData.id = actionHasId ? actionId : QnUuid::createUuid();
    eventData.title = title;
    eventData.description = description(params);
    eventData.toolTip = tooltip(action);
    eventData.helpId = rules::eventHelpId(params.eventType);
    eventData.level = QnNotificationLevel::valueOf(action);
    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.icon = pixmapForAction(action, eventData.titleColor);
    eventData.timestamp = timestamp;
    eventData.removable = true;
    eventData.ruleId = action->getRuleId();
    eventData.source = resource;

    eventData.objectTrackId = params.objectTrackId;
    eventData.attributes = qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
        params.getAnalyticsObjectTypeId(),
        params.attributes);

    if (actionType == ActionType::playSoundAction)
    {
        const auto soundUrl = action->getParams().url;
        if (!m_itemsByLoadingSound.contains(soundUrl))
            workbenchContext()->instance<ServerNotificationCache>()->downloadFile(soundUrl);

        m_itemsByLoadingSound.insert(soundUrl, eventData.id);
    }
    else if (actionType == ActionType::showOnAlarmLayoutAction)
    {
        eventData.actionId = menu::OpenInAlarmLayoutAction;
        eventData.actionParameters = alarmCameras;
        eventData.previewCamera = camera;
        eventData.cameras = alarmCameras;
    }
    else if (actionType == ActionType::showPopupAction)
    {
        const bool hasViewPermission = resource &&
            system()->accessController()->hasPermissions(resource, Qn::ViewContentPermission);

        switch (params.eventType)
        {
            case EventType::poeOverBudgetEvent:
            case EventType::fanErrorEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = menu::ServerSettingsAction;
                eventData.actionParameters = menu::Parameters(resource);
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
                eventData.actionId = menu::CameraSettingsAction;
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
                eventData.actionId = menu::ServerSettingsAction;
                eventData.actionParameters = server;
                break;
            }

            case EventType::cameraIpConflictEvent:
            {
                const auto& webPageAddress = params.caption;
                eventData.actionId = menu::BrowseUrlAction;
                eventData.actionParameters = {Qn::UrlRole, webPageAddress};
                break;
            }

            case EventType::licenseIssueEvent:
            {
                eventData.actionId = menu::PreferencesLicensesTabAction;
                break;
            }

            case EventType::userDefinedEvent:
            {
                const auto sourceCameras = camera_id_helper::findCamerasByFlexibleId(
                    system()->resourcePool(), params.metadata.cameraRefs).filtered(
                        [this](const auto& camera)
                        {
                            return system()->accessController()->hasPermissions(camera,
                                Qn::ViewContentPermission);
                        });

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

        if (camera)
            setupAcknowledgeAction(eventData, camera->getId(), action);
    }
    else
    {
        NX_ASSERT(false, "Unexpected action type: %1", actionType);
        return;
    }

    if (eventData.removable && actionType != ActionType::playSoundAction)
        eventData.lifetime = kDisplayTimeout;

    if (!q->addEvent(eventData))
        return;

    if (!actionHasId)
        m_uuidHashes[action->getRuleId()][eventData.sourceId()].insert(eventData.id);

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
            eventData.actionId = menu::ServerSettingsAction;
            eventData.actionParameters = menu::Parameters(server);
            eventData.actionParameters
                .setArgument(Qn::FocusTabRole, QnServerSettingsDialog::PoePage);
            break;

        case ClientAction::cameraSettings:
            eventData.actionId = menu::CameraSettingsAction;
            eventData.actionParameters = camera;
            eventData.previewCamera = camera;
            break;

        case ClientAction::serverSettings:
            eventData.actionId = menu::ServerSettingsAction;
            eventData.actionParameters = server;
            break;

        case ClientAction::licensesSettings:
            eventData.actionId = menu::PreferencesLicensesTabAction;
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
            eventData.actionId = menu::BrowseUrlAction;
            eventData.actionParameters = {Qn::UrlRole, action->url()};
            break;

        default:
            NX_ASSERT(false, "Unsupported client action");
            break;
    }
}

void NotificationListModel::Private::removeNotification(const vms::event::AbstractActionPtr& action)
{
    if (const auto actionId = action->getParams().actionId; !actionId.isNull())
    {
        q->removeEvent(actionId);
        return;
    }

    const auto ruleId = action->getRuleId();

    if (action->actionType() == ActionType::playSoundAction)
    {
        removeAllItems(ruleId);
        return;
    }

    const auto iter = m_uuidHashes.find(ruleId);
    if (iter == m_uuidHashes.end())
        return;

    const auto resourceId = action->getRuntimeParams().eventResourceId;

    for (const auto id: iter->value(resourceId)) //< Must iterate a copy of the list.
        q->removeEvent(id);
}

void NotificationListModel::Private::removeAllItems(QnUuid ruleId)
{
    if (auto it = m_uuidHashes.find(ruleId); it != m_uuidHashes.end())
    {
        QnUuidSet itemsToRemove;
        for (const auto& idSet: *it)
            itemsToRemove.unite(idSet);

        for (const auto id: itemsToRemove)
            q->removeEvent(id);
    }
}

void NotificationListModel::Private::setupAcknowledgeAction(EventData& eventData,
    const QnUuid& cameraId, const nx::vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != vms::api::ActionType::showPopupAction)
    {
        NX_ASSERT(false, "Invalid action type");
        return;
    }

    auto& actionParams = action->getParams();
    if (!actionParams.requireConfirmation(action->getRuntimeParams().eventType))
        return;

    if (!NX_ASSERT(!cameraId.isNull()))
        return;

    const auto camera = system()->resourcePool()->getResourceById(cameraId);
    if (!camera || !system()->accessController()->hasPermissions(camera,
        Qn::ManageBookmarksPermission))
    {
        return;
    }

    eventData.removable = false;
    eventData.level = QnNotificationLevel::Value::CriticalNotification;

    eventData.extraAction = CommandActionPtr(new CommandAction());

    static const QColor kBasicColor = "#A5B7C0";
    static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
        {QnIcon::Normal, {{kBasicColor, "light10"}}},
    };
    eventData.extraAction->setIcon(qnSkin->icon("buttons/acknowledge_24.svg", kIconSubstitutions));
    eventData.extraAction->setText(tr("Acknowledge"));

    // TODO: #sivanov Fix hardcoded action parameters.
    static const auto kDurationMs = std::chrono::milliseconds(std::chrono::seconds(10));
    actionParams.durationMs = kDurationMs.count();
    actionParams.recordBeforeMs = 0;

    const auto actionHandler =
        [this, cameraId, action]()
        {
            // TODO: FIXME! Could permissions have changed by this moment?
            menu::Parameters params;
            const auto camera = system()->resourcePool()->getResourceById(cameraId);
            if (camera && camera->systemContext() && !camera->hasFlags(Qn::removed))
                params.setResources({camera});
            params.setArgument(Qn::ActionDataRole, action);
            menu()->trigger(menu::AcknowledgeEventAction, params);
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
    const Qn::ResourceInfoLevel resourceInfoLevel =
        appContext()->localSettings()->resourceInfoLevel();

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
            if (auto camera = system()->resourcePool()->getResourceById<QnVirtualCameraResource>(id))
            {
                if (system()->accessController()->hasPermissions(camera,
                    Qn::ViewContentPermission))
                {
                    tooltip << QnResourceDisplayInfo(camera).toString(resourceInfoLevel);
                }
            }
        }
    }

    return tooltip.join(nx::vms::common::html::kLineBreak);
}

QPixmap NotificationListModel::Private::pixmapForAction(
    const vms::event::AbstractActionPtr& action, const QColor& color) const
{
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
        return qnSkin->icon("events/sound_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
        return qnSkin->icon("events/alarm_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));

    const auto& params = action->getRuntimeParams();

    if (params.eventType >= EventType::userDefinedEvent)
    {
        const auto camList = camera_id_helper::findCamerasByFlexibleId(
            system()->resourcePool(),
            params.metadata.cameraRefs);
        return camList.isEmpty()
            ? qnSkin->icon("events/alert_20.svg", kIconSubstitutions).pixmap(QSize(20, 20))
            : toPixmap(qnResIconCache->icon(QnResourceIconCache::Camera));
    }

    switch (params.eventType)
    {
        case EventType::cameraInputEvent:
        case EventType::cameraIpConflictEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
        {
            const auto resource = system()->resourcePool()->getResourceById(params.eventResourceId);
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
            return qnSkin->icon("events/storage_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));

        case EventType::cameraDisconnectEvent:
        case EventType::networkIssueEvent:
        {
            return qnSkin->icon("events/connection_20.svg", kIconSubstitutions)
                .pixmap(QSize(20, 20));
        }

        case EventType::serverStartEvent:
        case EventType::serverFailureEvent:
        case EventType::serverConflictEvent:
        case EventType::backupFinishedEvent:
        case EventType::serverCertificateError:
            return qnSkin->icon("events/server_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));

        case EventType::licenseIssueEvent:
            return qnSkin->icon("events/license_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));

        case EventType::pluginDiagnosticEvent:
            return qnSkin->icon("events/alert_20.svg", kIconSubstitutions).pixmap(QSize(20, 20));

        default:
            return QPixmap();
    }
}

QPixmap NotificationListModel::Private::pixmapForAction(
    const nx::vms::rules::NotificationAction* action,
    const QString& cloudSystemId,
    const QColor& color) const
{
    QnResourceList devices;
    if (needIconDevices(action->icon()))
        devices = getActionDevices(action, cloudSystemId);

    return eventIcon(action->icon(), action->level(), action->customIcon(), color, devices);
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
