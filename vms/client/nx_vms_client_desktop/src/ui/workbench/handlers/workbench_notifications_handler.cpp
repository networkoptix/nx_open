#include "workbench_notifications_handler.h"

#include <chrono>

#include <QtCore/QtGlobal>
#include <QtGui/QGuiApplication>

#include <QtWidgets/QAction>

#include <api/global_settings.h>
#include <api/app_server_connection.h>
#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/client/desktop/ui/actions/action_parameters.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_state_manager.h>

#include <utils/resource_property_adaptors.h>
#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>
#include <utils/camera/bookmark_helpers.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/actions/common_action.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <camera/camera_bookmarks_manager.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_conversion_functions.h>

using std::chrono::milliseconds;

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

using namespace nx::vms::event;

QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this))
{
    auto sessionDelegate = new QnBasicWorkbenchStateDelegate<QnWorkbenchNotificationsHandler>(this);
    static_cast<void>(sessionDelegate); //< Debug?

    connect(context(), &QnWorkbenchContext::userChanged,
        this, &QnWorkbenchNotificationsHandler::at_context_userChanged);

    QnCommonMessageProcessor* messageProcessor = commonModule()->messageProcessor();
    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        &QnWorkbenchNotificationsHandler::at_eventManager_actionReceived);

    connect(action(action::AcknowledgeEventAction), &QAction::triggered,
        this, &QnWorkbenchNotificationsHandler::handleAcknowledgeEventAction);
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler()
{
}

void QnWorkbenchNotificationsHandler::handleAcknowledgeEventAction()
{
    const auto actionParams = menu()->currentParameters(sender());
    const auto businessAction =
        actionParams.argument<vms::event::AbstractActionPtr>(Qn::ActionDataRole);
    const auto camera = actionParams.resource().dynamicCast<QnVirtualCameraResource>();

    auto bookmark = helpers::bookmarkFromAction(businessAction, camera);
    if (!bookmark.isValid())
        return;

    const QScopedPointer<QnCameraBookmarkDialog> bookmarksDialog(
        new QnCameraBookmarkDialog(true, mainWindowWidget()));

    bookmark.description = QString(); //< Force user to fill description out.
    bookmarksDialog->loadData(bookmark);
    if (bookmarksDialog->exec() != QDialog::Accepted)
        return;

    const auto creationCallback =
        [this, businessAction, parentThreadId = QThread::currentThreadId()](bool success)
        {
            if (QThread::currentThreadId() != parentThreadId)
            {
                NX_ASSERT(false, "Invalid thread!");
                return;
            }

            if (!success)
                return;

            const auto action = CommonAction::createBroadcastAction(
                ActionType::showPopupAction, businessAction->getParams());
            action->setToggleState(nx::vms::api::EventState::inactive);

            if (const auto connection = commonModule()->ec2Connection())
            {
                static const auto fakeHandler = [](int /*handle*/, ec2::ErrorCode /*errorCode*/){};

                const auto manager = connection->makeEventRulesManager(Qn::kSystemAccess);
                nx::vms::api::EventActionData actionData;
                ec2::fromResourceToApi(action, actionData);
                manager->broadcastEventAction(actionData, this, fakeHandler);
            }
        };

    bookmarksDialog->submitData(bookmark);

    const auto currentUserId = context()->user()->getId();
    const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    bookmark.creatorId = currentUserId;
    bookmark.creationTimeStampMs = milliseconds(currentTimeMs);

    qnCameraBookmarksManager->addAcknowledge(bookmark, businessAction->getRuleId(),
        creationCallback);

    // Hiding notification instantly to keep UX smooth.
    emit notificationRemoved(businessAction);
}

void QnWorkbenchNotificationsHandler::handleFullscreenCameraAction(
    const nx::vms::event::AbstractActionPtr& action)
{
    const auto params = action->getParams();
    const auto camera = resourcePool()->getResourceById(params.actionResourceId).dynamicCast<
        QnVirtualCameraResource>();
    NX_ASSERT(camera);
    if (!camera)
        return;

    const auto currentLayout = workbench()->currentLayout();
    const auto layoutResource = currentLayout->resource();
    if (!layoutResource)
        return;

    const auto resources = action->getResources();
    const bool layoutIsAllowed = resources.contains(layoutResource->getId());
    if (!layoutIsAllowed)
        return;

    auto items = currentLayout->items(camera);
    if (items.empty())
        return;

    workbench()->setItem(Qn::ZoomedRole, *items.cbegin());
}

void QnWorkbenchNotificationsHandler::handleExitFullscreenAction(
    const nx::vms::event::AbstractActionPtr& action)
{
    const auto currentLayout = workbench()->currentLayout();
    const auto layoutResource = currentLayout->resource();
    if (!layoutResource)
        return;

    const bool layoutIsAllowed = action->getResources().contains(layoutResource->getId());
    if (!layoutIsAllowed)
        return;

    workbench()->setItem(Qn::ZoomedRole, nullptr);
}

void QnWorkbenchNotificationsHandler::clear()
{
    emit cleared();
}

void QnWorkbenchNotificationsHandler::addNotification(const vms::event::AbstractActionPtr &action)
{
    vms::event::EventParameters params = action->getRuntimeParams();
    vms::api::EventType eventType = params.eventType;

    if (eventType >= vms::api::EventType::systemHealthEvent && eventType <= vms::api::EventType::maxSystemHealthEvent)
    {
        int healthMessage = eventType - vms::api::EventType::systemHealthEvent;
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), action);
        return;
    }

    if (!context()->user())
        return;

    if (action->actionType() == vms::api::ActionType::showOnAlarmLayoutAction)
    {
        /* Skip action if it contains list of users, and we are not on the list. */
        if (!QnBusiness::actionAllowedForUser(action, context()->user()))
            return;
    }

    bool alwaysNotify = false;
    switch (action->actionType())
    {
        case vms::api::ActionType::showOnAlarmLayoutAction:
        case vms::api::ActionType::playSoundAction:
            //case vms::event::playSoundOnceAction: -- handled outside without notification
            alwaysNotify = true;
            break;

        default:
            break;
    }

    if (!alwaysNotify && !m_adaptor->isAllowed(eventType))
        return;

    emit notificationAdded(action);
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message)
{
    addSystemHealthEvent(message, vms::event::AbstractActionPtr());
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message,
    const vms::event::AbstractActionPtr &action)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(action), true);
}

bool QnWorkbenchNotificationsHandler::tryClose(bool /*force*/)
{
    clear();
    return true;
}

void QnWorkbenchNotificationsHandler::forcedUpdate()
{
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible)
{
    setSystemHealthEventVisibleInternal(message, QVariant(), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message,
    const QnResourcePtr &resource,
    bool visible)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(resource), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisibleInternal(
    QnSystemHealth::MessageType message,
    const QVariant& params,
    bool visible)
{
    bool canShow = true;

    const bool connected = !commonModule()->remoteGUID().isNull();

    if (!connected)
    {
        canShow = false;
        if (visible)
        {
            /* In unit tests there can be users when we are disconnected. */
            QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);
            if (guiApp)
                NX_ASSERT(false, "No events should be displayed if we are disconnected");
        }
    }
    else
    {
        /* Only admins can see system health events handled here. */
        if (!accessController()->hasGlobalPermission(GlobalPermission::admin))
            canShow = false;
    }

    /* Some messages are not to be displayed to users. */
    canShow &= (QnSystemHealth::isMessageVisible(message));

    // TODO: remove this hack in VMS-7724
    auto filterMessageKey = message;
    if (message == QnSystemHealth::RemoteArchiveSyncFinished
        || message == QnSystemHealth::RemoteArchiveSyncProgress
        || message == QnSystemHealth::RemoteArchiveSyncError)
    {
        filterMessageKey = QnSystemHealth::RemoteArchiveSyncStarted;
    }

    // Checking that user wants to see this message (if he is able to hide it).
    if (isMessageVisibleInSettings(filterMessageKey))
    {
        const bool isAllowedByFilter = qnSettings->popupSystemHealth().contains(filterMessageKey);
        canShow &= isAllowedByFilter;
    }

    if (visible && canShow)
        emit systemHealthEventAdded(message, params);
    else
        emit systemHealthEventRemoved(message, params);
}

void QnWorkbenchNotificationsHandler::at_context_userChanged()
{
    m_adaptor->setResource(context()->user());
}

void QnWorkbenchNotificationsHandler::at_eventManager_actionReceived(
    const vms::event::AbstractActionPtr& action)
{
    if (!QnBusiness::actionAllowedForUser(action, context()->user()))
        return;

    if (!QnBusiness::hasAccessToSource(action->getRuntimeParams(), context()->user()))
        return;

    switch (action->actionType())
    {
        case vms::api::ActionType::showOnAlarmLayoutAction:
            addNotification(action);
            break;
        case vms::api::ActionType::playSoundOnceAction:
        {
            QString filename = action->getParams().url;
            QString filePath = context()->instance<ServerNotificationCache>()->getFullPath(filename);
            // If file doesn't exist then it's already deleted or not downloaded yet.
            // I think it should not be played when downloaded.
            AudioPlayer::playFileAsync(filePath);
            break;
        }

        case vms::api::ActionType::showPopupAction: //< Fallthrough
        case vms::api::ActionType::playSoundAction:
        {
            switch (action->getToggleState())
            {
                case vms::api::EventState::undefined:
                case vms::api::EventState::active:
                    addNotification(action);
                    break;

                case vms::api::EventState::inactive:
                    emit notificationRemoved(action);
                    break;

                default:
                    break;
            }
            break;
        }

        case vms::api::ActionType::sayTextAction:
        {
            AudioPlayer::sayTextAsync(action->getParams().sayText);
            break;
        }

        case ActionType::fullscreenCameraAction:
        {
            handleFullscreenCameraAction(action);
            break;
        }

        case ActionType::exitFullscreenAction:
        {
            handleExitFullscreenAction(action);
            break;
        }

        default:
            break;
    }
}
