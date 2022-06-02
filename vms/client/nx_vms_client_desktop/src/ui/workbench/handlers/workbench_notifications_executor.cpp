// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_notifications_executor.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::rules;

QnWorkbenchNotificationsExecutor::QnWorkbenchNotificationsExecutor(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    nx::vms::client::desktop::appContext()->currentSystemContext()->vmsRulesEngine()
        ->addActionExecutor(utils::type<NotificationAction>(), this);
}

void QnWorkbenchNotificationsExecutor::execute(const ActionPtr& action)
{
    if (action->type() == utils::type<NotificationAction>())
    {
        auto notificationAction = action.dynamicCast<NotificationAction>();
        if (!NX_ASSERT(notificationAction))
            return;

        auto user = context()->user();
        if (!user)
            return;

        const auto isUserSelected =
            [&]()
            {
                auto usersSelection = notificationAction->users();
                if (usersSelection.all || usersSelection.ids.contains(user->getId()))
                    return true;

                for (const auto roleId: user->allUserRoleIds())
                {
                    if (usersSelection.ids.contains(roleId))
                        return true;
                }

                return false;

            };

        if (isUserSelected())
            emit notificationActionReceived(notificationAction);
    }
}
