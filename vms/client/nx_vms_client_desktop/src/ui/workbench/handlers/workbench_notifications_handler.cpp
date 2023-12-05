// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_notifications_handler.h"

#include <chrono>

#include <QtGui/QAction>
#include <QtGui/QGuiApplication>

#include <business/business_resource_validation.h>
#include <camera/camera_bookmarks_manager.h>
#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/resource/property_adaptors.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/camera/bookmark_helpers.h>
#include <utils/common/synctime.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>

using namespace std::chrono;

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::event;

namespace {

QString toString(vms::event::AbstractActionPtr action)
{
    return nx::format(
        "{actionType: %1, toggleState: %2, receivedFromRemoteHost: %3, resources: %4, params: %5, "
            "runtimeParams: %6, ruleId: %7, aggregationCount: %8}",
        action->actionType(),
        action->getToggleState(),
        action->isReceivedFromRemoteHost(),
        action->getResources(),
        nx::reflect::json::serialize(action->getParams()),
        nx::reflect::json::serialize(action->getRuntimeParams()),
        action->getRuleId().toSimpleString(),
        action->getAggregationCount());
}

} // namespace

QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_adaptor(new nx::vms::common::BusinessEventFilterResourcePropertyAdaptor(this))
{
    auto sessionDelegate = new QnBasicWorkbenchStateDelegate<QnWorkbenchNotificationsHandler>(this);
    static_cast<void>(sessionDelegate); //< Debug?

    context()->instance<ServerNotificationCache>();

    connect(context(), &QnWorkbenchContext::userChanged,
        this, &QnWorkbenchNotificationsHandler::at_context_userChanged);

    QnCommonMessageProcessor* messageProcessor = systemContext()->messageProcessor();
    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        &QnWorkbenchNotificationsHandler::at_businessActionReceived);

    connect(action(action::AcknowledgeEventAction), &QAction::triggered,
        this, &QnWorkbenchNotificationsHandler::handleAcknowledgeEventAction);

    connect(this, &QnWorkbenchNotificationsHandler::notificationAdded,
        this,
        [this](const nx::vms::event::AbstractActionPtr& action)
        {
            NX_VERBOSE(this, "A notification is added: %1", toString(action));
        });

    connect(this, &QnWorkbenchNotificationsHandler::notificationRemoved,
        this,
        [this](const nx::vms::event::AbstractActionPtr& action)
        {
            NX_VERBOSE(this, "A notification is removed: %1", toString(action));
        });
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

            if (const auto connection = messageBusConnection())
            {
                const auto manager = connection->getEventRulesManager(Qn::kSystemAccess);
                nx::vms::api::EventActionData actionData;
                ec2::fromResourceToApi(action, actionData);
                manager->broadcastEventAction(actionData, [](int /*handle*/, ec2::ErrorCode) {});
            }
        };

    if (!camera || !camera->systemContext())
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Unable to acknowledge event on removed camera."));

        creationCallback(true);

        // Hiding notification instantly to keep UX smooth.
        removeNotification(businessAction);
        return;
    }

    auto bookmark = helpers::bookmarkFromAction(businessAction, camera);
    if (!bookmark.isValid())
        return;

    const QScopedPointer<QnCameraBookmarkDialog> bookmarksDialog(
        new QnCameraBookmarkDialog(true, mainWindowWidget()));

    bookmark.description = QString(); //< Force user to fill description out.
    bookmarksDialog->loadData(bookmark);
    if (bookmarksDialog->exec() != QDialog::Accepted)
        return;

    bookmarksDialog->submitData(bookmark);

    const auto currentUserId = context()->user()->getId();
    const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    bookmark.creatorId = currentUserId;
    bookmark.creationTimeStampMs = milliseconds(currentTimeMs);

    auto systemContext = SystemContext::fromResource(camera);
    systemContext->cameraBookmarksManager()->addAcknowledge(
        bookmark,
        businessAction->getRuleId(),
        creationCallback);

    // Hiding notification instantly to keep UX smooth.
    removeNotification(businessAction);
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
    auto iter = std::find_if(items.begin(), items.end(),
        [](const QnWorkbenchItem* item)
        {
            return item->zoomRect().isNull();
        });

    if (iter != items.end())
    {
        workbench()->setItem(Qn::ZoomedRole, *iter);
        if (params.recordBeforeMs > 0)
        {
            const microseconds navigationTime = qnSyncTime->currentTimePoint()
                - milliseconds(params.recordBeforeMs);

            using namespace ui::action;
            menu()->triggerIfPossible(JumpToTimeAction,
                Parameters().withArgument(Qn::TimestampRole, navigationTime));
        }
    }
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
    const auto eventType = action->getRuntimeParams().eventType;

    if (eventType >= vms::api::EventType::systemHealthEvent
        && eventType <= vms::api::EventType::maxSystemHealthEvent)
    {
        int healthMessage = eventType - vms::api::EventType::systemHealthEvent;
        addSystemHealthEvent(MessageType(healthMessage), action);
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

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(
    MessageType message, const vms::event::AbstractActionPtr& action)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(action), true);
}

void QnWorkbenchNotificationsHandler::removeNotification(
    const vms::event::AbstractActionPtr& action)
{
    const vms::api::EventType eventType = action->getRuntimeParams().eventType;

    if (eventType >= vms::api::EventType::systemHealthEvent
        && eventType <= vms::api::EventType::maxSystemHealthEvent)
    {
        const int healthMessage = eventType - vms::api::EventType::systemHealthEvent;

        setSystemHealthEventVisibleInternal(
            MessageType(healthMessage), QVariant::fromValue(action), false);
    }
    else
    {
        emit notificationRemoved(action);
    }
}

bool QnWorkbenchNotificationsHandler::tryClose(bool /*force*/)
{
    clear();
    return true;
}

void QnWorkbenchNotificationsHandler::forcedUpdate()
{
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(
    MessageType message, const QnResourcePtr& resource, bool visible)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(resource), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisibleInternal(
    MessageType message, const QVariant& params, bool visible)
{
    bool canShow = true;

    const bool connected = (bool) context()->user();

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
        if (!accessController()->hasPowerUserPermissions())
            canShow = false;
    }

    /* Some messages are not to be displayed to users. */
    canShow &= (nx::vms::common::system_health::isMessageVisible(message));

    // Checking that user wants to see this message (if he is able to hide it).
    if (isMessageVisibleInSettings(message))
    {
        const bool isAllowedByFilter =
            appContext()->localSettings()->popupSystemHealth().contains(message);
        canShow &= isAllowedByFilter;
    }

    NX_VERBOSE(this,
        "A system health event is %1: %2",
        (visible && canShow) ? "added" : "removed",
        message);

    if (visible && canShow)
        emit systemHealthEventAdded(message, params);
    else
        emit systemHealthEventRemoved(message, params);
}

void QnWorkbenchNotificationsHandler::at_context_userChanged()
{
    const auto user = context()->user();

    m_adaptor->setResource(user);

    if (!user)
        clear();
}

void QnWorkbenchNotificationsHandler::at_businessActionReceived(
    const vms::event::AbstractActionPtr& action)
{
    NX_VERBOSE(this, "An action is received: %1", toString(action));

    const auto user = context()->user();
    if (!user)
    {
        NX_DEBUG(this, "The user has logged out already");
        return;
    }

    if (!QnBusiness::actionAllowedForUser(action, user))
    {
        NX_VERBOSE(this, "The action is not allowed for the user %1", user->getName());

        return;
    }

    if (!vms::event::hasAccessToSource(action->getRuntimeParams(), user))
    {
        NX_VERBOSE(
            this, "User %1 has no access to the action's source", user->getName());

        return;
    }

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
                    removeNotification(action);
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
