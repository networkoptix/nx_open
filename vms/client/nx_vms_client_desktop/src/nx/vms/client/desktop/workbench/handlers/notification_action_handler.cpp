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
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
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
#include <nx/vms/event/helpers.h>
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
using namespace nx::vms::api;
using namespace nx::vms::common::system_health;

namespace {

bool messageAllowedForUser(const SiteHealthMessage& message, const QnUserResourcePtr& user)
{
    if (!user)
        return false;

    const auto systemContext = user->systemContext()->as<SystemContext>();
    if (!NX_ASSERT(systemContext))
        return false;

    if (isMessageIntercom(message.type))
        return true;

    return systemContext->accessController()->hasPowerUserPermissions();
}

std::string toString(const SiteHealthMessage& message)
{
    return nx::reflect::json::serialize(message);
}

bool isMessageWatched(const nx::vms::api::UserSettings& settings,
    nx::vms::common::system_health::MessageType messageType)
{
    return !settings.messageFilter.contains(reflect::toString(messageType));
}

/** Check whether the user has an access to this event. */
bool hasAccessToSource(const SiteHealthMessage& message, const QnUserResourcePtr& user)
{
    if (message.resourceIds.empty())
        return true;

    const auto context = user->systemContext();
    const auto resources = context->resourcePool()->getResourcesByIds(message.resourceIds);

    for (const auto& resource: resources)
    {
        if (context->resourceAccessManager()->hasPermission(
            user, resource, Qn::ViewContentPermission))
        {
            NX_VERBOSE(NX_SCOPE_TAG, "%1 has permission for the event from %2", user, resource);
            return true;
        }
    }

    NX_VERBOSE(NX_SCOPE_TAG, "%1 has not permission for the event from %2",
        user, containerString(resources));
    return false;
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
    connect(messageProcessor, &QnClientMessageProcessor::hardwareIdMappingRemoved, this,
        [this](nx::Uuid id)
        {
            onSystemHealthMessage(SiteHealthMessage{
                .type = SiteHealthMessageType::replacedDeviceDiscovered,
                .active = false,
                .resourceIds = {id}
            });
        });

    const auto eventConnector = systemContext->serverRuntimeEventConnector();
    connect(eventConnector,
        &nx::vms::client::core::ServerRuntimeEventConnector::serviceDisabled,
        this,
        &NotificationActionHandler::at_serviceDisabled);
    connect(eventConnector,
        &nx::vms::client::core::ServerRuntimeEventConnector::siteHealthMessage,
        this,
        &NotificationActionHandler::onSystemHealthMessage);
}

NotificationActionHandler::~NotificationActionHandler()
{
}

void NotificationActionHandler::clear()
{
    emit cleared();
}

void NotificationActionHandler::removeNotification(const SiteHealthMessage& message)
{
    NX_VERBOSE(this, "Removing notification: %1", toString(message));
    NX_ASSERT(!message.active);

    emit systemHealthEventRemoved(message);
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
    SiteHealthMessage message;
    switch (reason)
    {
        case EventReason::notEnoughLocalRecordingServices:
            message.type = SiteHealthMessageType::recordingServiceDisabled;
            break;
        case EventReason::notEnoughCloudRecordingServices:
            message.type = SiteHealthMessageType::cloudServiceDisabled;
            break;
        case EventReason::notEnoughIntegrationServices:
            message.type = SiteHealthMessageType::integrationServiceDisabled;
            break;
        default:
            NX_ASSERT(false, "Unexpected event reason %1", reason);
            return;
    }

    message.resourceIds.assign(deviceIds.cbegin(), deviceIds.cend());

    onSystemHealthMessage(message);
}

void NotificationActionHandler::onSystemHealthMessage(const SiteHealthMessage& message)
{
    NX_VERBOSE(this, "Site health message received: %1", toString(message));

    const auto user = system()->user();
    if (!user)
    {
        NX_DEBUG(this, "The user has logged out already");
        return;
    }

    if (!isMessageVisible(message.type))
    {
        NX_VERBOSE(this, "The message is not visible: %1", message.type);
        return;
    }

    if (!messageAllowedForUser(message, user))
    {
        NX_VERBOSE(this, "The message is not allowed for the user %1", user->getName());

        return;
    }

    if (!hasAccessToSource(message, user))
    {
        NX_VERBOSE(
            this, "User %1 has no access to the message source", user->getName());

        return;
    }

    if (!isMessageWatched(user->settings(), message.type))
    {
        NX_VERBOSE(this, "The message is not watched by the user: %1", message.type);
        return;
    }

    if (message.active)
    {
        NX_VERBOSE(this, "Adding Site health message: %1", toString(message));
        emit systemHealthEventAdded(message);
    }
    else
    {
        removeNotification(message);
    }
}

} // namespace nx::vms::client::desktop
