// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "call_notifications_list_model_p.h"

#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
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
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common::system_health;

namespace {

bool messageIsSupported(MessageType message)
{
    return message == MessageType::showIntercomInformer
        || message == MessageType::showMissedCallInformer;
}

} // namespace

using nx::vms::api::ActionType;

CallNotificationsListModel::Private::Private(CallNotificationsListModel* q):
    base_type(),
    WindowContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(system()))
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

    // Id is fixed and equal to intercom id, so this informer will be removed
    // when another client instance via changing toogle state.
    eventData.id = runtimeParameters.eventResourceId;

    switch (message)
    {
        case MessageType::showIntercomInformer:
        {
            eventData.actionId = menu::OpenIntercomLayoutAction;
            eventData.title = tr("Calling...");
            eventData.icon = qnSkin->pixmap("events/call.svg");
            const auto intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(camera);
            eventData.actionParameters =
                system()->resourcePool()->getResourceById(intercomLayoutId);
            eventData.removable = false;
            break;
        }
        case MessageType::showMissedCallInformer:
        {
            eventData.actionId = menu::NoAction;
            eventData.title = tr("Missed Call");
            eventData.icon = qnSkin->pixmap("events/missed_call.svg");
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
