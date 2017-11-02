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

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

namespace {

const auto kDisplayTimeout = std::chrono::milliseconds(12500);

QPixmap toPixmap(const QIcon& icon)
{
    return QnSkin::maximumSizePixmap(icon);
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

    connect(q, &EventListModel::modelReset, this, [this]() { m_uuidHashes.clear(); });
}

NotificationListModel::Private::~Private()
{
}

NotificationListModel::Private::ExtraData NotificationListModel::Private::extraData(
    const EventData& event)
{
    NX_ASSERT(event.extraData.canConvert<ExtraData>(), Q_FUNC_INFO);
    return event.extraData.value<ExtraData>();
}

void NotificationListModel::Private::beforeRemove(const EventData& event)
{
    const auto extraData = Private::extraData(event);
    m_uuidHashes[extraData.first].remove(extraData.second);
}

void NotificationListModel::Private::addNotification(const vms::event::AbstractActionPtr& action)
{
    const auto& params = action->getRuntimeParams();
    const auto ruleId = action->getRuleId();

    auto title = m_helper->eventAtResource(params, qnSettings->extraInfoInTree());
    const auto timestampMs = params.eventTimestampUsec / 1000;

    QnResourcePtr resource = resourcePool()->getResourceById(params.eventResourceId);
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    const bool hasViewPermission = resource && accessController()->hasPermissions(resource,
        Qn::ViewContentPermission);

    auto alarmCameras = resourcePool()->getResources<QnVirtualCameraResource>(action->getResources());
    if (action->getParams().useSource)
        alarmCameras << resourcePool()->getResources<QnVirtualCameraResource>(action->getSourceResources());
    alarmCameras = accessController()->filtered(alarmCameras, Qn::ViewContentPermission);
    alarmCameras = alarmCameras.toSet().toList();

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
    {
        if (alarmCameras.isEmpty())
            return;

        const auto iter = m_uuidHashes.find(ruleId);
        if (iter != m_uuidHashes.end())
        {
            for (const auto& id: *iter)
            {
                if (q->indexOf(id).data(Qn::TimestampRole).value<qint64>() == timestampMs)
                    return;
            }
        }

        title = tr("Alarm: %1").arg(title);
    }

    QStringList tooltip = m_helper->eventDescription(action,
        vms::event::AggregationInfo(), qnSettings->extraInfoInTree());

    // TODO: #GDM #3.1 move this code to ::eventDetails()
    if (params.eventType == vms::event::licenseIssueEvent
        && params.reasonCode == vms::event::EventReason::licenseRemoved)
    {
        QStringList disabledCameras;
        for (const QString& stringId : params.description.split(L';'))
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
    eventData.timestamp = timestampMs;
    eventData.removable = true;
    eventData.extraData = qVariantFromValue(ExtraData(action->getRuleId(), resource));
    eventData.icon = pixmapForAction(action);

    eventData.titleColor = QnNotificationLevel::notificationColor(
        QnNotificationLevel::valueOf(action));

    // TODO: FIXME: #vkutin Restore functionality.
#if 0
    switch (action->actionType())
    {
        // TODO: #GDM not the best place for it.
        case vms::event::playSoundAction:
        {
            QString soundUrl = action->getParams().url;
            m_itemsByLoadingSound.insert(soundUrl, item);
            context()->instance<ServerNotificationCache>()->downloadFile(soundUrl);
            break;
        }
        default:
            break;
    };
#endif

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
    {
        eventData.actionId = action::OpenInAlarmLayoutAction;
        eventData.actionParameters = alarmCameras;

        //loadThumbnailForItem(item, alarmCameras.mid(0, kMaxThumbnailCount));
    }
    else
    {
        switch (params.eventType)
        {
            case vms::event::cameraMotionEvent:
            case vms::event::softwareTriggerEvent:
            case vms::event::analyticsSdkEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::OpenInNewTabAction;
                eventData.actionParameters = action::Parameters(camera)
                    .withArgument(Qn::ItemTimeRole, timestampMs);
                //loadThumbnailForItem(item, camera, timestampMs);
                setupAcknowledgeAction(eventData, camera, action);
                break;
            }

            case vms::event::cameraInputEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::OpenInNewTabAction;
                eventData.actionParameters = camera;
                //loadThumbnailForItem(item, camera);
                setupAcknowledgeAction(eventData, camera, action);
                break;
            }

            case vms::event::cameraDisconnectEvent:
            case vms::event::networkIssueEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::CameraSettingsAction;
                eventData.actionParameters = camera;
                //loadThumbnailForItem(item, camera);
                setupAcknowledgeAction(eventData, camera, action);
                break;
            }

            case vms::event::storageFailureEvent:
            case vms::event::backupFinishedEvent:
            case vms::event::serverStartEvent:
            case vms::event::serverFailureEvent:
            {
                NX_ASSERT(hasViewPermission);
                eventData.actionId = action::ServerSettingsAction;
                eventData.actionParameters = server;
                break;
            }

            case vms::event::cameraIpConflictEvent:
            {
                const auto& webPageAddress = params.caption;
                eventData.actionId = action::BrowseUrlAction;
                eventData.actionParameters = {Qn::UrlRole, webPageAddress};
                break;
            }

            case vms::event::licenseIssueEvent:
            {
                eventData.actionId = action::PreferencesLicensesTabAction;
                break;
            }

            case vms::event::userDefinedEvent:
            {
                auto sourceCameras = resourcePool()->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
                sourceCameras = accessController()->filtered(sourceCameras, Qn::ViewContentPermission);
                if (!sourceCameras.isEmpty())
                {
                    eventData.actionId = action::OpenInNewTabAction;
                    eventData.actionParameters = action::Parameters(sourceCameras)
                        .withArgument(Qn::ItemTimeRole, timestampMs);

                    //loadThumbnailForItem(item, sourceCameras.mid(0, kMaxThumbnailCount), timestampMs);
                    setupAcknowledgeAction(eventData, sourceCameras.first(), action);
                }
                break;
            }

            default:
                break;
        }
    }

    if (!q->addEvent(eventData))
        return;

    if (!actionHasId)
        m_uuidHashes[action->getRuleId()][resource] = eventData.id;

    const bool isPlaySoundAction = action->actionType() == vms::event::playSoundAction;
    if (!isPlaySoundAction && eventData.removable)
    {
        executeDelayedParented([this, id = eventData.id]() { q->removeEvent(id); },
            std::chrono::milliseconds(kDisplayTimeout).count(), this);
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

    const auto ruleId = action->getRuleId();
    const auto iter = m_uuidHashes.find(ruleId);
    if (iter == m_uuidHashes.end())
        return;

    const auto& uuidHash = *iter;

    if (action->actionType() == vms::event::playSoundAction)
    {
        for (const auto& id: uuidHash.values()) //< Must iterate a copy of the list.
            q->removeEvent(id);

        return;
    }

    const auto resource = resourcePool()->getResourceById(action->getRuntimeParams().eventResourceId);
    if (!resource)
        return;

    if (uuidHash.contains(resource))
        q->removeEvent(uuidHash.value(resource));
}

void NotificationListModel::Private::setupAcknowledgeAction(EventData& eventData,
    const QnVirtualCameraResourcePtr& camera,
    const nx::vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != vms::event::ActionType::showPopupAction)
    {
        NX_ASSERT(false, "Invalid action type.");
        return;
    }

    if (!context()->accessController()->hasGlobalPermission(Qn::GlobalManageBookmarksPermission))
        return;

    auto& actionParams = action->getParams();
    if (!actionParams.requireConfirmation(action->getRuntimeParams().eventType))
        return;

    if (!camera)
        return;

    NX_EXPECT(menu()->canTrigger(action::AcknowledgeEventAction, camera));
    if (!menu()->canTrigger(action::AcknowledgeEventAction, camera))
        return;

    eventData.removable = false;

    // TODO: FIXME: #vkutin Restore functionality. This was used for sorting.
    //widget->setNotificationLevel(QnNotificationLevel::Value::CriticalNotification);

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
    const vms::event::AbstractActionPtr& action) const
{
    if (action->actionType() == vms::event::playSoundAction)
        return qnSkin->pixmap("events/sound.png");

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
        return qnSkin->pixmap("events/alarm.png");

    const auto& params = action->getRuntimeParams();

    if (params.eventType >= vms::event::userDefinedEvent)
    {
        const auto camList = resourcePool()->getResources<QnVirtualCameraResource>(
            params.metadata.cameraRefs);
        return camList.isEmpty()
            ? QPixmap()
            : toPixmap(qnResIconCache->icon(QnResourceIconCache::Camera));
    }

    switch (params.eventType)
    {
        case vms::event::cameraMotionEvent:
        case vms::event::cameraInputEvent:
        case vms::event::cameraDisconnectEvent:
        case vms::event::cameraIpConflictEvent:
        case vms::event::networkIssueEvent:
        case vms::event::analyticsSdkEvent:
        {
            const auto resource = resourcePool()->getResourceById(params.eventResourceId);
            return toPixmap(resource
                ? qnResIconCache->icon(resource)
                : qnResIconCache->icon(QnResourceIconCache::Camera));
        }

        case vms::event::softwareTriggerEvent:
        {
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                action->getRuntimeParams().description,
                QPalette().light().color());
        }

        case vms::event::storageFailureEvent:
            return qnSkin->pixmap("events/storage.png");

        case vms::event::serverStartEvent:
        case vms::event::serverFailureEvent:
        case vms::event::serverConflictEvent:
        case vms::event::backupFinishedEvent:
            return toPixmap(qnResIconCache->icon(QnResourceIconCache::Server));

        case vms::event::licenseIssueEvent:
            return qnSkin->pixmap("events/license.png");

        default:
            return QPixmap();
    }
}

} // namespace
} // namespace client
} // namespace nx
