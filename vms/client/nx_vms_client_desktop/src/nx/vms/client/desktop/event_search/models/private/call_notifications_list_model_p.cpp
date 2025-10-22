// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "call_notifications_list_model_p.h"

#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/site_health_message.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/camera/buttons/intercom_helper.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/intercom/utils.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;
using namespace nx::vms::api;

namespace {

bool messageIsSupported(SiteHealthMessageType message)
{
    return nx::vms::common::system_health::isMessageIntercom(message);
}

void postRejectIntercomCall(
    SystemContext* system,
    nx::Uuid intercomId,
    nx::utils::AsyncHandlerExecutor executor)
{
    if (!NX_ASSERT(system))
        return;

    auto serverApi = system->connectedServerApi();
    if (!NX_ASSERT(serverApi))
        return;

    serverApi->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        system->getSessionTokenHelper(),
        nx::network::http::Method::post,
        nx::format("/rest/v4/devices/%1/intercom/rejectCall", intercomId.toSimpleString()),
        nx::network::rest::Params(),
        /*body*/ QByteArray(),
        [intercomId](
            bool success,
            rest::Handle /*requestId*/,
            const rest::ServerConnection::ErrorOrEmpty& result)
        {
            if (success)
                return;

            NX_LOG_RESPONSE(NX_SCOPE_TAG, success, result,
                "The call rejection operation for the %1 intercom has failed.",
                intercomId.toString());
        },
        executor);
}

} // namespace

using nx::vms::api::ActionType;

CallNotificationsListModel::Private::Private(CallNotificationsListModel* q):
    base_type(),
    WindowContextAware(q),
    q(q),
    m_soundController(this)
{
    const auto handler = windowContext()->notificationActionHandler();

    connect(handler, &NotificationActionHandler::cleared, q, &EventListModel::clear);

    connect(handler, &NotificationActionHandler::systemHealthEventAdded,
        this, &Private::addNotification);
    connect(handler, &NotificationActionHandler::systemHealthEventRemoved,
        this, &Private::removeNotification);
}

CallNotificationsListModel::Private::~Private()
{
}

void CallNotificationsListModel::Private::addNotification(
    const nx::vms::api::SiteHealthMessage& message)
{
    if (!messageIsSupported(message.type))
        return; //< Message type is processed in a separate model.

    if (!NX_ASSERT(system()))
        return;

    const auto camera = system()->resourcePool()->getResourceById<QnVirtualCameraResource>(message.resourceId());

    EventData eventData;
    eventData.toolTip = tooltip(message, camera);
    eventData.helpId = HelpTopic::EventsActions_ShowIntercomInformer;
    eventData.level = QnNotificationLevel::valueOf(system(), message.type);
    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.timestamp = message.timestampUs;
    eventData.ruleId = message.resourceId();
    eventData.source = camera;
    eventData.previewCamera = camera;
    // Using layout as main resource for standard layout handlers to work properly.
    eventData.actionParameters = system()->resourcePool()->getResourceById(
        nx::vms::common::calculateIntercomLayoutId(camera));
    eventData.removable = true;

    switch (message.type)
    {
        case SiteHealthMessageType::showIntercomInformer:
        {
            // Id is fixed and equal to intercom id, so this informer will be removed
            // on every client instance via changing toggle state.
            eventData.id = message.resourceId();

            eventData.actionId = menu::OpenIntercomLayoutAction;
            eventData.title = tr("Calling...");
            eventData.iconPath = "20x20/Outline/call.svg";
            eventData.level = QnNotificationLevel::Value::SuccessNotification;

            eventData.extraAction = CommandActionPtr(new CommandAction());
            eventData.extraAction->setText(tr("Open"));

            const auto openDoorActionHandler =
                [this, camera]()
                {
                    if (!system()->ioPortsInterface())
                        return;

                    system()->ioPortsInterface()->setIoPortState(
                        system()->getSessionTokenHelper(),
                        camera,
                        api::ExtendedCameraOutput::powerRelay,
                        /*isActive*/ true,
                        /*autoResetTimeout*/ IntercomHelper::kOpenedDoorDuration);
                };

            connect(eventData.extraAction.data(), &CommandAction::triggered,
                [this, openDoorActionHandler]() { executeLater(openDoorActionHandler, this); });

            eventData.onCloseAction = CommandActionPtr(new CommandAction());

            const auto rejectCallActionHandler =
                [this, cameraId = camera->getId()]()
                {
                    postRejectIntercomCall(system(), cameraId, thread());
                };

            connect(eventData.onCloseAction.data(), &CommandAction::triggered,
                [this, rejectCallActionHandler]() { executeLater(rejectCallActionHandler, this); });

            m_soundController.play("doorbell.mp3");

            break;
        }
        case SiteHealthMessageType::showMissedCallInformer:
        {
            eventData.id = nx::Uuid::createUuid(); //< We have to show every missed call separately.
            eventData.actionId = menu::OpenMissedCallIntercomLayoutAction;
            eventData.title = tr("Missed call");
            eventData.iconPath = "20x20/Outline/missed_call.svg";
            eventData.level = QnNotificationLevel::Value::CriticalNotification;
            eventData.actionParameters.setArgument(Qn::ItemUuidRole, eventData.id);

            break;
        }
        default:
        {
            NX_ASSERT(false, "Unexpected message type: %1", message.type);
        }
    }

    eventData.actionParameters.setArgument(nx::vms::client::core::UuidRole, message.resourceId());

    // Recreate informer on the top in case of several calls in a short period.
    if (message.type == SiteHealthMessageType::showIntercomInformer)
        q->removeEvent(eventData.id);

    if (!q->addEvent(eventData))
        return;
}

void CallNotificationsListModel::Private::removeNotification(
    const SiteHealthMessage& message)
{
    if (!messageIsSupported(message.type))
        return; //< Message type is processed in a separate model.

    const auto id = message.resourceId();
    if (id.isNull())
        return;

    q->removeEvent(id);
}

QString CallNotificationsListModel::Private::tooltip(
    const SiteHealthMessage& message,
    const QnVirtualCameraResourcePtr& camera) const
{
    QStringList tooltip;
    if (message.type == SiteHealthMessageType::showIntercomInformer)
        tooltip << tr("Call Request");
    else if (message.type == SiteHealthMessageType::showMissedCallInformer)
        tooltip << tr("Call Request Missed");

    if (camera)
    {
        tooltip << tr("Source: %1").arg(QnResourceDisplayInfo(camera)
            .toString(appContext()->localSettings()->resourceInfoLevel()));
    }

    return tooltip.join(nx::vms::common::html::kLineBreak);
}

} // namespace nx::vms::client::desktop
