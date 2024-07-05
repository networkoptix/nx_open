// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "call_notifications_list_model_p.h"

#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/camera/buttons/intercom_helper.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/vms/event/actions/system_health_action.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/strings_helper.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <transaction/message_bus_adapter.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common::system_health;

namespace {

bool messageIsSupported(MessageType message)
{
    return message == MessageType::showIntercomInformer
        || message == MessageType::showMissedCallInformer;
}

bool setRelayOutputId(const QnVirtualCameraResourcePtr camera,
    api::ExtendedCameraOutput cameraOutput,
    nx::vms::event::ActionParameters* actionParameters)
{
    const auto portDescriptions = camera->ioPortDescriptions();
    const auto portIter =
        std::find_if(portDescriptions.begin(), portDescriptions.end(),
            [cameraOutput](const QnIOPortData& portData)
            {
                return portData.outputName == QString::fromStdString(
                    nx::reflect::toString(cameraOutput));
            });
    if (portIter == portDescriptions.end())
        return false;

    actionParameters->relayOutputId = portIter->id;
    return true;
}

} // namespace

using nx::vms::api::ActionType;

CallNotificationsListModel::Private::Private(CallNotificationsListModel* q):
    base_type(),
    WindowContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(system())),
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
    MessageType message,
    const QVariant& params)
{
    if (!messageIsSupported(message))
        return; //< Message type is processed in a separate model.

    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    vms::event::AbstractActionPtr action;
    if (params.canConvert<vms::event::AbstractActionPtr>())
        action = params.value<vms::event::AbstractActionPtr>();

    if (!NX_ASSERT(action))
        return;

    const auto& runtimeParameters = action->getRuntimeParams();

    const QnResourcePtr cameraResource = system()->resourcePool()->getResourceById(
        runtimeParameters.eventResourceId);
    const auto camera = cameraResource.dynamicCast<QnVirtualCameraResource>();

    EventData eventData;
    eventData.toolTip = tooltip(action);
    eventData.helpId = rules::eventHelpId(runtimeParameters.eventType);
    eventData.level = QnNotificationLevel::valueOf(message);
    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.timestamp = (std::chrono::microseconds) runtimeParameters.eventTimestampUsec;
    eventData.ruleId = runtimeParameters.eventResourceId;
    eventData.source = camera;
    eventData.previewCamera = camera;

    switch (message)
    {
        case MessageType::showIntercomInformer:
        {
            // Id is fixed and equal to intercom id, so this informer will be removed
            // on every client instance via changing toggle state.
            eventData.id = runtimeParameters.eventResourceId;

            eventData.actionId = menu::OpenIntercomLayoutAction;
            eventData.title = tr("Calling...");
            eventData.iconPath = "16x16/Solid/phone.svg";
            const auto intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(camera);
            eventData.actionParameters =
                system()->resourcePool()->getResourceById(intercomLayoutId);
            eventData.removable = true;

            eventData.extraAction = CommandActionPtr(new CommandAction());
            eventData.extraAction->setText(tr("Open"));

            const auto openDoorActionHandler =
                [this, action, camera]()
                {
                    nx::vms::event::ActionParameters actionParameters;

                    if (!setRelayOutputId(
                        camera, api::ExtendedCameraOutput::powerRelay, &actionParameters))
                    {
                        // Is possible when the camera is removed while the call is in progress.
                        return;
                    }

                    actionParameters.durationMs =
                        nx::vms::client::core::IntercomHelper::kOpenedDoorDuration.count();

                    nx::vms::api::EventActionData actionData;
                    actionData.actionType = nx::vms::api::ActionType::cameraOutputAction;
                    actionData.toggleState = nx::vms::api::EventState::active;
                    actionData.resourceIds.push_back(camera->getId());
                    actionData.params = QJson::serialized(actionParameters);

                    auto callback = nx::utils::guarded(this,
                        [this](
                            bool success,
                            rest::Handle /*requestId*/,
                            nx::network::rest::JsonResult result)
                        {
                            if (!success)
                                NX_WARNING(this, "Open door operation was unsuccessful.");
                        });

                    if (auto connection = q->system()->connectedServerApi())
                        connection->executeEventAction(actionData, callback, thread());
                };

            connect(eventData.extraAction.data(), &CommandAction::triggered,
                [this, openDoorActionHandler]() { executeLater(openDoorActionHandler, this); });

            eventData.onCloseAction = CommandActionPtr(new CommandAction());

            const auto rejectCallActionHandler =
                [this, camera]()
                {
                    vms::event::SystemHealthActionPtr broadcastAction(
                        new vms::event::SystemHealthAction(
                            MessageType::rejectIntercomCall,
                            camera->getId()));

                    if (const auto connection = system()->messageBusConnection())
                    {
                        const auto manager = connection->getEventRulesManager(
                            nx::network::rest::kSystemSession);
                        nx::vms::api::EventActionData actionData;
                        ec2::fromResourceToApi(broadcastAction, actionData);
                        manager->broadcastEventAction(actionData,
                            [](int /*handle*/, ec2::ErrorCode) {});
                    }
                };

            connect(eventData.onCloseAction.data(), &CommandAction::triggered,
                [this, rejectCallActionHandler]() { executeLater(rejectCallActionHandler, this); });

            m_soundController.play("doorbell.mp3");

            break;
        }
        case MessageType::showMissedCallInformer:
        {
            eventData.id = nx::Uuid::createUuid(); //< We have to show every missed call separately.
            eventData.actionId = menu::NoAction;
            eventData.title = tr("Missed Call");
            eventData.iconPath = "16x16/Solid/missed.svg";
            eventData.removable = true;
            break;
        }
        default:
        {
            NX_ASSERT(false, "Unexpected message type: %1", message);
        }
    }

    eventData.actionParameters.setArgument(Qn::ActionDataRole, action);

    // Recreate informer on the top in case of several calls in a short period.
    if (message == MessageType::showIntercomInformer)
        q->removeEvent(eventData.id);

    if (!q->addEvent(eventData))
        return;
}

void CallNotificationsListModel::Private::removeNotification(
    MessageType message,
    const QVariant& params)
{
    if (!messageIsSupported(message))
        return; //< Message type is processed in a separate model.

    vms::event::AbstractActionPtr action;
    if (params.canConvert<vms::event::AbstractActionPtr>())
        action = params.value<vms::event::AbstractActionPtr>();

    if (!NX_ASSERT(action))
        return;

    const auto id = action->getRuntimeParams().eventResourceId;
    if (id.isNull())
        return;

    q->removeEvent(id);
}

QString CallNotificationsListModel::Private::tooltip(
    const vms::event::AbstractActionPtr& action) const
{
    const QStringList tooltip = m_helper->eventDescription(
        action,
        vms::event::AggregationInfo(),
        appContext()->localSettings()->resourceInfoLevel(),
        nx::vms::event::AttrSerializePolicy::none);

    return tooltip.join(nx::vms::common::html::kLineBreak);
}

} // namespace nx::vms::client::desktop
