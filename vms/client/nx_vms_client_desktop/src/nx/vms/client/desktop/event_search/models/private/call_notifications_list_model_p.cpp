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

using nx::vms::api::ActionType;

CallNotificationsListModel::Private::Private(CallNotificationsListModel* q):
    base_type(),
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
}

CallNotificationsListModel::Private::~Private()
{
}

void CallNotificationsListModel::Private::addNotification(
    const vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != ActionType::showIntercomInformer)
        return;

    NX_VERBOSE(this,
        "Received action: %1, id: %2",
        action->actionType(),
        action->getParams().actionId);

    const auto& params = action->getRuntimeParams();

    const QnResourcePtr resource = system()->resourcePool()->getResourceById(params.eventResourceId);
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();

    EventData eventData;
    eventData.toolTip = tooltip(action);
    eventData.helpId = rules::eventHelpId(params.eventType);
    eventData.level = QnNotificationLevel::valueOf(action);
    eventData.timestamp = (std::chrono::microseconds) params.eventTimestampUsec;
    eventData.ruleId = action->getRuleId();
    eventData.source = resource;
    eventData.objectTrackId = params.objectTrackId;

    eventData.attributes = qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
        params.getAnalyticsObjectTypeId(), params.attributes);

    // Id is fixed and equal to intercom id, so this informer will be removed
    // when another client instance via changing toogle state.
    eventData.id = action->getParams().actionResourceId;
    eventData.actionId = menu::OpenIntercomLayoutAction;
    eventData.title = tr("Calling...");
    const auto intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(camera);
    eventData.actionParameters = system()->resourcePool()->getResourceById(intercomLayoutId);
    eventData.actionParameters.setArgument(Qn::ActionDataRole, action);
    eventData.previewCamera = camera;
    eventData.removable = false;

    eventData.titleColor = QnNotificationLevel::notificationTextColor(eventData.level);
    eventData.icon = qnSkin->pixmap("events/call.svg");

    // Recreate informer on the top in case of several calls in a short period.
    q->removeEvent(eventData.id);

    if (!q->addEvent(eventData))
        return;
}

void CallNotificationsListModel::Private::removeNotification(const vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != ActionType::showIntercomInformer)
        return;

    const auto id = action->getParams().actionResourceId;
    if (id.isNull())
        return;

    q->removeEvent(id);
}

QString CallNotificationsListModel::Private::tooltip(const vms::event::AbstractActionPtr& action) const
{
    const QStringList tooltip = m_helper->eventDescription(
        action,
        vms::event::AggregationInfo(),
        appContext()->localSettings()->resourceInfoLevel(),
        nx::vms::event::AttrSerializePolicy::none);

    return tooltip.join(nx::vms::common::html::kLineBreak);
}

} // namespace nx::vms::client::desktop
