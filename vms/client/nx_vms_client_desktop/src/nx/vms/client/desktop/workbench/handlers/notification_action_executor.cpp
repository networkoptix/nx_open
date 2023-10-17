// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_action_executor.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/desktop/rules/cross_system_notifications_listener.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::rules;

NotificationActionExecutor::NotificationActionExecutor(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    auto engine = systemContext()->vmsRulesEngine();

    engine->addActionExecutor(rules::utils::type<NotificationAction>(), this);
    engine->addActionExecutor(rules::utils::type<RepeatSoundAction>(), this);
    engine->addActionExecutor(rules::utils::type<ShowOnAlarmLayoutAction>(), this);

    connect(context(), &QnWorkbenchContext::userChanged,
        this, &NotificationActionExecutor::onContextUserChanged);
}

NotificationActionExecutor::~NotificationActionExecutor()
{}

void NotificationActionExecutor::onContextUserChanged()
{
    using namespace nx::vms;

    const auto user = context()->user();

    if (user && user->isCloud()
        && (common::saas::saasServicesOperational(systemContext())
            || rules::ini().enableCSNwithoutSaaS))
    {
        m_listener = std::make_unique<CrossSystemNotificationsListener>();
        connect(m_listener.get(),
            &CrossSystemNotificationsListener::notificationActionReceived,
            this,
            &NotificationActionExecutor::notificationActionReceived);
    }
    else
    {
        m_listener.reset();
    }
}

void NotificationActionExecutor::execute(const ActionPtr& action)
{
    if (!vms::rules::checkUserPermissions(context()->user(), action))
        return;

    auto notificationAction = action.dynamicCast<NotificationActionBase>();
    if (!NX_ASSERT(notificationAction, "Unexpected action: %1", action->type()))
        return;

    emit notificationActionReceived(notificationAction, {});

    // Show splash.
    QSet<QnResourcePtr> targetResources;

    if (!notificationAction->deviceIds().empty())
    {
        targetResources = nx::utils::toQSet(
            resourcePool()->getResourcesByIds(notificationAction->deviceIds()));
    }
    else
    {
        targetResources.insert(resourcePool()->getResourceById(notificationAction->serverId()));
    }

    targetResources.remove({});

    display()->showNotificationSplash(
        targetResources.values(), QnNotificationLevel::convert(notificationAction->level()));
}

} // namespace nx::vms::client::desktop
