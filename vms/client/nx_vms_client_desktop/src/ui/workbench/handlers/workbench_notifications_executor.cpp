// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_notifications_executor.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/audio/audiodevice.h>
#include <nx/utils/qset.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>
#include <utils/media/audio_player.h>

using namespace nx::vms::rules;

namespace {

// TODO: #amalov Consider moving this check to client router.
// TODO: #amalov Check access to source.
bool checkUserPermissions(const QnUserResourcePtr& user, const ActionPtr& action)
{
    if (!user)
        return false;

    const auto propValue = action->property(utils::kUsersFieldName);
    if (!propValue.isValid() || !propValue.canConvert<UuidSelection>())
        return false;

    const auto userSelection = propValue.value<UuidSelection>();
    if (userSelection.all || userSelection.ids.contains(user->getId()))
        return true;

    const auto userRoles = nx::utils::toQSet(user->allUserRoleIds());
    return userRoles.intersects(userSelection.ids);
}

} // namespace

QnWorkbenchNotificationsExecutor::QnWorkbenchNotificationsExecutor(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    auto engine =
        nx::vms::client::desktop::appContext()->currentSystemContext()->vmsRulesEngine();

    engine->addActionExecutor(utils::type<NotificationAction>(), this);
    engine->addActionExecutor(utils::type<SpeakAction>(), this);
}

void QnWorkbenchNotificationsExecutor::execute(const ActionPtr& action)
{
    if (!checkUserPermissions(context()->user(), action))
        return;

    if (action->type() == utils::type<NotificationAction>())
    {
        auto notificationAction = action.dynamicCast<NotificationAction>();
        if (!NX_ASSERT(notificationAction))
            return;

        emit notificationActionReceived(notificationAction);
    }
    else if (action->type() == utils::type<SpeakAction>())
    {
        auto speakAction = action.dynamicCast<SpeakAction>();
        if (!NX_ASSERT(speakAction))
            return;

        if (const auto text = speakAction->text(); !text.isEmpty())
        {
            nx::audio::AudioDevice::instance()->setVolume(speakAction->volume());
            AudioPlayer::sayTextAsync(speakAction->text());
        }
    }
}
