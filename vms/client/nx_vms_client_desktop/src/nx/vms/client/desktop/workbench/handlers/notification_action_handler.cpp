// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_action_handler.h"

#include <chrono>

#include <QtGui/QAction>
#include <QtGui/QGuiApplication>

#include <camera/camera_bookmarks_manager.h>
#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/bookmark/bookmark_helpers.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/utils/event.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/synctime.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

using namespace nx::vms::event;

namespace {

bool actionAllowedForUser(
    const nx::vms::event::AbstractActionPtr& action,
    const QnUserResourcePtr& user)
{
    if (!user)
        return false;

    const auto eventType = action->getRuntimeParams().eventType;
    if (eventType >= nx::vms::api::EventType::siteHealthEvent
        && eventType <= nx::vms::api::EventType::maxSiteHealthEvent)
    {
        const auto healthMessage = nx::vms::common::system_health::MessageType(
            eventType - nx::vms::api::EventType::siteHealthEvent);

        if (healthMessage == nx::vms::common::system_health::MessageType::showIntercomInformer
            || healthMessage == nx::vms::common::system_health::MessageType::showMissedCallInformer)
        {
            const auto& runtimeParameters = action->getRuntimeParams();

            const auto systemContext =
                qobject_cast<nx::vms::client::desktop::SystemContext*>(user->systemContext());
            if (!NX_ASSERT(systemContext))
                return false;

            const QnResourcePtr cameraResource =
                systemContext->resourcePool()->getResourceById(runtimeParameters.eventResourceId);

            const bool hasViewPermission = cameraResource &&
                systemContext->accessController()->hasPermissions(
                    cameraResource,
                    Qn::ViewContentPermission);

            if (hasViewPermission)
                return true;
        }
    }

    switch (action->actionType())
    {
        case nx::vms::event::ActionType::fullscreenCameraAction:
        case nx::vms::event::ActionType::exitFullscreenAction:
            return true;
        default:
            break;
    }

    const auto params = action->getParams();
    if (params.allUsers)
        return true;

    const auto userId = user->getId();
    const auto& subjects = params.additionalResources;

    if (std::find(subjects.cbegin(), subjects.cend(), userId) != subjects.cend())
        return true;

    const auto userGroups = nx::vms::common::userGroupsWithParents(user);
    for (const auto& groupId: userGroups)
    {
        if (std::find(subjects.cbegin(), subjects.cend(), groupId) != subjects.cend())
            return true;
    }

    return false;
}

QString toString(AbstractActionPtr action)
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

bool isMessageWatched(const nx::vms::api::UserSettings& settings, const QString& messageType)
{
    return !settings.messageFilter.contains(messageType.toStdString());
}

bool isMessageWatched(const nx::vms::api::UserSettings& settings,
    nx::vms::common::system_health::MessageType messageType)
{
    return isMessageWatched(settings, QString::fromStdString(reflect::toString(messageType)));
}

} // namespace

NotificationActionHandler::NotificationActionHandler(
    WindowContext* windowContext, QObject *parent)
    :
    base_type(parent),
    WindowContextAware(windowContext)
{
    connect(windowContext, &WindowContext::beforeSystemChanged,
        this, &NotificationActionHandler::clear);

    const auto systemContext = system();
    connect(systemContext->userWatcher(), &nx::vms::client::core::UserWatcher::userChanged,
        this, &NotificationActionHandler::at_context_userChanged);

    const auto messageProcessor = systemContext->clientMessageProcessor();
    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        &NotificationActionHandler::at_businessActionReceived);

    connect(messageProcessor, &QnClientMessageProcessor::hardwareIdMappingRemoved, this,
        [this](const nx::Uuid& id)
        {
            const auto resource = system()->resourcePool()->getResourceById(id);
            setSystemHealthEventVisible(MessageType::replacedDeviceDiscovered, resource, false);
        });

    const auto eventConnector = systemContext->serverRuntimeEventConnector();
    connect(eventConnector,
        &nx::vms::client::core::ServerRuntimeEventConnector::serviceDisabled,
        this,
        &NotificationActionHandler::at_serviceDisabled);

    connect(this, &NotificationActionHandler::notificationAdded,
        this,
        [this](const AbstractActionPtr& action)
        {
            NX_VERBOSE(this, "A notification is added: %1", toString(action));
        });

    connect(this, &NotificationActionHandler::notificationRemoved,
        this,
        [this](const AbstractActionPtr& action)
        {
            NX_VERBOSE(this, "A notification is removed: %1", toString(action));
        });
}

NotificationActionHandler::~NotificationActionHandler()
{
}

void NotificationActionHandler::handleFullscreenCameraAction(
    const nx::vms::event::AbstractActionPtr& action)
{
    const auto params = action->getParams();
    const auto camera = system()->resourcePool()->getResourceById(params.actionResourceId)
        .dynamicCast<QnVirtualCameraResource>();

    if (!NX_ASSERT(camera))
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

            using namespace menu;
            menu()->triggerIfPossible(JumpToTimeAction,
                Parameters().withArgument(nx::vms::client::core::TimestampRole, navigationTime));
        }
    }
}

void NotificationActionHandler::handleExitFullscreenAction(
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

void NotificationActionHandler::clear()
{
    emit cleared();
}

void NotificationActionHandler::addNotification(const vms::event::AbstractActionPtr &action)
{
    const auto eventType = action->getRuntimeParams().eventType;
    const bool isSystemHealthEvent = eventType >= vms::api::EventType::siteHealthEvent
        && eventType <= vms::api::EventType::maxSiteHealthEvent;

    if (NX_ASSERT(isSystemHealthEvent, "Events from the old rules engine must not occur"))
    {
        int healthMessage = eventType - vms::api::EventType::siteHealthEvent;
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

    if (!alwaysNotify && !rules::isEventWatched(system()->user()->settings(), eventType))
        return;

    emit notificationAdded(action);

    showSplash(action);
}

void NotificationActionHandler::addSystemHealthEvent(
    MessageType message, const vms::event::AbstractActionPtr& action)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(action), true);
}

void NotificationActionHandler::setSystemHealthEventVisible(
    MessageType message, const QnResourcePtr& resource, bool visible)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(resource), visible);
}

void NotificationActionHandler::removeNotification(const vms::event::AbstractActionPtr& action)
{
    const vms::api::EventType eventType = action->getRuntimeParams().eventType;

    if (eventType >= vms::api::EventType::siteHealthEvent
        && eventType <= vms::api::EventType::maxSiteHealthEvent)
    {
        const int healthMessage = eventType - vms::api::EventType::siteHealthEvent;

        setSystemHealthEventVisibleInternal(
            MessageType(healthMessage), QVariant::fromValue(action), false);
    }
    else
    {
        emit notificationRemoved(action);
    }
}

void NotificationActionHandler::setSystemHealthEventVisibleInternal(
    MessageType message, const QVariant& params, bool visible)
{
    bool canShow = true;

    const bool connected = !system()->user().isNull();

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
        // Only admins can see system health events, except intercom call-related.
        if (message != MessageType::showIntercomInformer
            && message != MessageType::showMissedCallInformer
            && !system()->accessController()->hasPowerUserPermissions())
        {
            canShow = false;
        }
    }

    /* Some messages are not to be displayed to users. */
    canShow &= (nx::vms::common::system_health::isMessageVisible(message));

    // Checking that user wants to see this message (if he is able to hide it).
    if (isMessageVisibleInSettings(message))
    {
        bool isAllowedByFilter = false;
        if (auto user = system()->user())
            isAllowedByFilter = isMessageWatched(user->settings(), message);

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

void NotificationActionHandler::at_context_userChanged()
{
    if (!system()->user())
        clear();
}

void NotificationActionHandler::at_serviceDisabled(
    nx::vms::api::EventReason reason,
    const std::set<nx::Uuid>& deviceIds)
{
    using namespace nx::vms::api;
    MessageType messageType;
    switch (reason)
    {
        case EventReason::notEnoughLocalRecordingServices:
            messageType = MessageType::recordingServiceDisabled;
            break;
        case EventReason::notEnoughCloudRecordingServices:
            messageType = MessageType::cloudServiceDisabled;
            break;
        case EventReason::notEnoughIntegrationServices:
            messageType = MessageType::integrationServiceDisabled;
            break;
        default:
            NX_ASSERT(false, "Unexpected event reason %1", reason);
            return;
    }

    auto resources = system()->resourcePool()->getResourcesByIds(deviceIds);
    setSystemHealthEventVisibleInternal(messageType, QVariant::fromValue(resources), true);
}

void NotificationActionHandler::at_businessActionReceived(
    const vms::event::AbstractActionPtr& action)
{
    NX_ASSERT(action->actionType() == nx::vms::api::ActionType::showPopupAction,
        "Old engine is disabled, so only system health events should get here.");

    NX_VERBOSE(this, "An action is received: %1", toString(action));

    const auto user = system()->user();
    if (!user)
    {
        NX_DEBUG(this, "The user has logged out already");
        return;
    }

    if (!actionAllowedForUser(action, user))
    {
        NX_VERBOSE(this, "The action is not allowed for the user %1", user->getName());

        return;
    }

    if (!hasAccessToSource(action->getRuntimeParams(), user))
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
            const QString filename = action->getParams().url;
            const QString filePath = system()->serverNotificationCache()->getFullPath(filename);
            // If file doesn't exist then it's already deleted or not downloaded yet.
            // I think it should not be played when downloaded.
            AudioPlayer::playFileAsync(filePath);
            break;
        }

        // IMPORTANT: System Health Events are coming as showPopupAction.
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

void NotificationActionHandler::showSplash(
    const vms::event::AbstractActionPtr& businessAction)
{
    /*
    * We are displaying notifications in the two use cases:
    * on ShowOnAlarmLayoutAction
    * and on ShowPopupAction (including prolonged PlaySoundAction as its subtype)
    * / showIntercomInformer / showMissedCallInformer.
    * In first case we got at_notificationsHandler_businessActionAdded called once for each camera, that
    * should be displayed on the alarm layout (including source cameras and custom cameras if required).
    * In second case we should manually collect resources from event sources.
    */
    QSet<QnResourcePtr> targetResources;
    vms::api::ActionType actionType = businessAction->actionType();
    if (actionType == vms::api::ActionType::showOnAlarmLayoutAction)
    {
        if (const auto resource = system()->resourcePool()->getResourceById(
            businessAction->getParams().actionResourceId))
        {
            targetResources.insert(resource);
        }
    }
    else
    {
        NX_ASSERT(actionType == vms::api::ActionType::showPopupAction
            || actionType == vms::api::ActionType::playSoundAction);
        vms::event::EventParameters eventParams = businessAction->getRuntimeParams();

        if (const auto resource = system()->resourcePool()->getResourceById(
            eventParams.eventResourceId))
        {
            targetResources.insert(resource);
        }

        if (eventParams.eventType >= vms::api::EventType::userDefinedEvent)
        {
            QnResourceList cameras = system()->resourcePool()->getCamerasByFlexibleIds(
                eventParams.metadata.cameraRefs);
            targetResources.unite({cameras.cbegin(), cameras.cend()});
        }
    }

    display()->showNotificationSplash(
        targetResources.values(), QnNotificationLevel::valueOf(businessAction));
}

} // namespace nx::vms::client::desktop
