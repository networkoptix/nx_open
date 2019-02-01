#include "notification_list_model_p.h"

#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/common/notification_levels.h>
#include <ui/help/business_help.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <utils/common/delayed.h>
#include <utils/media/audio_player.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::vms::client::desktop {

using namespace ui;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {

static const auto kDisplayTimeout = std::chrono::milliseconds(12500);

QPixmap toPixmap(const QIcon& icon)
{
    return QnSkin::maximumSizePixmap(icon);
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
                connect(player, &AudioPlayer::done, player, &AudioPlayer::deleteLater);
            else
                player->deleteLater();
        });
}

} // namespace

NotificationListModel::Private::Private(NotificationListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule()))
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
        });

    connect(q, &EventListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int row = first; row <= last; ++row)
            {
                const auto& event = this->q->getEvent(row);
                const auto extraData = Private::extraData(event);
                m_uuidHashes[extraData.first][extraData.second].remove(event.id);
                m_players.remove(event.id);
            }
    });

    connect(context()->instance<ServerNotificationCache>(),
        &ServerNotificationCache::fileDownloaded, this,
        [this](const QString& fileName)
        {
            const auto path = context()->instance<ServerNotificationCache>()->getFullPath(fileName);
            for (const auto& id: m_itemsByLoadingSound.values(fileName))
                m_players[id] = loopSound(path);

            m_itemsByLoadingSound.remove(fileName);
        });
}

NotificationListModel::Private::~Private()
{
}

NotificationListModel::Private::ExtraData NotificationListModel::Private::extraData(
    const EventData& event)
{
    NX_ASSERT(event.extraData.canConvert<ExtraData>());
    return event.extraData.value<ExtraData>();
}

void NotificationListModel::Private::addNotification(const vms::event::AbstractActionPtr& action)
{
    using namespace std::chrono;

    const auto& params = action->getRuntimeParams();
    const auto ruleId = action->getRuleId();

    auto title = m_helper->eventAtResource(params, qnSettings->extraInfoInTree());
    const microseconds timestamp(params.eventTimestampUsec);
    const qint64 timestampMs = duration_cast<milliseconds>(timestamp).count();

    QnResourcePtr resource = resourcePool()->getResourceById(params.eventResourceId);
    auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    const bool hasViewPermission = resource && accessController()->hasPermissions(resource,
        Qn::ViewContentPermission);

    auto alarmCameras =
        resourcePool()->getResourcesByIds<QnVirtualCameraResource>(action->getResources());
    if (action->getParams().useSource)
    {
        const auto sourceResourceIds = action->getSourceResources(resourcePool());
        alarmCameras.append(resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
            sourceResourceIds));
    }
    alarmCameras = accessController()->filtered(alarmCameras, Qn::ViewContentPermission);
    alarmCameras = alarmCameras.toSet().toList();

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
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

    QStringList tooltip = m_helper->eventDescription(action,
        vms::event::AggregationInfo(), qnSettings->extraInfoInTree());

    // TODO: #GDM #3.1 move this code to ::eventDetails()
    if (params.eventType == EventType::licenseIssueEvent
        && params.reasonCode == vms::api::EventReason::licenseRemoved)
    {
        QStringList disabledCameras;
        for (const auto& stringId: params.description.split(L';'))
        {
            QnUuid id = QnUuid::fromStringSafe(stringId);
            NX_ASSERT(!id.isNull());
            if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id))
            {
                if (accessController()->hasPermissions(camera, Qn::ViewContentPermission))
                    tooltip << QnResourceDisplayInfo(camera).toString(qnSettings->extraInfoInTree());
            }
        }
    }

    const auto actionId = action->getParams().actionId;
    const bool actionHasId = !actionId.isNull();

    EventData eventData;
    eventData.id = actionHasId ? actionId : QnUuid::createUuid();
    eventData.title = title;
    eventData.toolTip = tooltip.join(lit("<br>"));
    eventData.helpId = QnBusiness::eventHelpId(params.eventType);
    eventData.level = QnNotificationLevel::valueOf(action);
    eventData.timestamp = timestamp;
    eventData.removable = true;
    eventData.extraData = qVariantFromValue(ExtraData(action->getRuleId(), resource));

    if (action->actionType() == ActionType::playSoundAction)
    {
        const auto soundUrl = action->getParams().url;
        if (!m_itemsByLoadingSound.contains(soundUrl))
            context()->instance<ServerNotificationCache>()->downloadFile(soundUrl);

        m_itemsByLoadingSound.insert(soundUrl, eventData.id);
    }

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
    {
        eventData.actionId = action::OpenInAlarmLayoutAction;
        eventData.actionParameters = alarmCameras;
        eventData.previewCamera = camera;
        eventData.cameras = alarmCameras;
    }
    else
    {
        switch (params.eventType)
        {
            case EventType::cameraMotionEvent:
            case EventType::softwareTriggerEvent:
            case EventType::analyticsSdkEvent:
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
                auto sourceCameras = camera_id_helper::findCamerasByFlexibleId(
                    resourcePool(),
                    params.metadata.cameraRefs);
                sourceCameras = accessController()->filtered(sourceCameras, Qn::ViewContentPermission);
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

    if (eventData.removable && action->actionType() != ActionType::playSoundAction)
        eventData.lifetime = kDisplayTimeout;

    if (action->actionType() == ActionType::showPopupAction && camera)
        setupAcknowledgeAction(eventData, camera, action);

    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.icon = pixmapForAction(action, eventData.titleColor);

    if (!q->addEvent(eventData))
        return;

    if (!actionHasId)
        m_uuidHashes[action->getRuleId()][resource].insert(eventData.id);
}

void NotificationListModel::Private::removeNotification(const vms::event::AbstractActionPtr& action)
{
    const auto actionId = action->getParams().actionId;
    if (!actionId.isNull())
    {
        q->removeEvent(actionId);
        return;
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
            removeEvents(ids.toList());

        return;
    }

    const auto resource = resourcePool()->getResourceById(action->getRuntimeParams().eventResourceId);
    if (!resource)
        return;

    if (uuidHash.contains(resource))
        removeEvents(uuidHash[resource].toList());
}

void NotificationListModel::Private::setupAcknowledgeAction(EventData& eventData,
    const QnVirtualCameraResourcePtr& camera,
    const nx::vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != vms::api::ActionType::showPopupAction)
    {
        NX_ASSERT(false, "Invalid action type");
        return;
    }

    if (!context()->accessController()->hasGlobalPermission(GlobalPermission::manageBookmarks))
        return;

    auto& actionParams = action->getParams();
    if (!actionParams.requireConfirmation(action->getRuntimeParams().eventType))
        return;

    if (!camera)
        return;

    NX_ASSERT(menu()->canTrigger(action::AcknowledgeEventAction, camera));
    if (!menu()->canTrigger(action::AcknowledgeEventAction, camera))
        return;

    eventData.removable = false;
    eventData.level = QnNotificationLevel::Value::CriticalNotification;

    eventData.extraAction = CommandActionPtr(new CommandAction(this));
    eventData.extraAction->setIcon(qnSkin->icon("buttons/acknowledge.png"));
    eventData.extraAction->setText(tr("Acknowledge"));

    // Modify action parameters. This looks like a hack.
    static const auto kDurationMs = std::chrono::milliseconds(std::chrono::seconds(10));
    actionParams.durationMs = kDurationMs.count();
    actionParams.recordBeforeMs = 0;

    const auto actionHandler =
        [this, camera, action]()
        {
            action::Parameters params(camera);
            params.setArgument(Qn::ActionDataRole, action);
            menu()->trigger(action::AcknowledgeEventAction, params);
        };

    connect(eventData.extraAction.data(), &QAction::triggered,
        [this, actionHandler]() { executeDelayedParented(actionHandler, 0, this); });
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
        return qnSkin->pixmap("events/sound.png");

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
        return qnSkin->pixmap("events/alarm.png");

    const auto& params = action->getRuntimeParams();

    if (params.eventType >= EventType::userDefinedEvent)
    {
        const auto camList = camera_id_helper::findCamerasByFlexibleId(
                    resourcePool(),
                    params.metadata.cameraRefs);
        return camList.isEmpty()
            ? QPixmap()
            : toPixmap(qnResIconCache->icon(QnResourceIconCache::Camera));
    }

    switch (params.eventType)
    {
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        case EventType::cameraIpConflictEvent:
        case EventType::analyticsSdkEvent:
        {
            const auto resource = resourcePool()->getResourceById(params.eventResourceId);
            return toPixmap(resource
                ? qnResIconCache->icon(resource)
                : qnResIconCache->icon(QnResourceIconCache::Camera));
        }

        case EventType::softwareTriggerEvent:
        {
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                action->getRuntimeParams().description,
                color.isValid() ? color : QPalette().light().color());
        }

        case EventType::storageFailureEvent:
            return qnSkin->pixmap("events/storage.png");

        case EventType::cameraDisconnectEvent:
        case EventType::networkIssueEvent:
            return qnSkin->pixmap("events/connection.png");

        case EventType::serverStartEvent:
        case EventType::serverFailureEvent:
        case EventType::serverConflictEvent:
        case EventType::backupFinishedEvent:
            return qnSkin->pixmap("events/server.png");

        case EventType::licenseIssueEvent:
            return qnSkin->pixmap("events/license.png");

        case EventType::pluginEvent:
            return qnSkin->pixmap("events/alert.png");

        default:
            return QPixmap();
    }
}

} // namespace nx::vms::client::desktop
