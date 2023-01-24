// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_notifications_executor.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/audio/audiodevice.h>
#include <nx/utils/qset.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/rules/actions/enter_fullscreen_action.h>
#include <nx/vms/rules/actions/exit_fullscreen_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/synctime.h>
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
    auto engine = systemContext()->vmsRulesEngine();

    engine->addActionExecutor(utils::type<EnterFullscreenAction>(), this);
    engine->addActionExecutor(utils::type<ExitFullscreenAction>(), this);
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
            auto callback = AudioPlayer::Callback();

            if (const auto volume = speakAction->volume(); volume >= 0)
            {
                auto device = nx::audio::AudioDevice::instance();
                callback = [device, volume = device->volume()] { device->setVolume(volume); };

                device->setVolume(speakAction->volume());
            }

            AudioPlayer::sayTextAsync(speakAction->text(), this, std::move(callback));
        }
    }
    else if (action->type() == utils::type<EnterFullscreenAction>())
    {
        auto enterFullscreenAction = action.dynamicCast<EnterFullscreenAction>();
        if (!NX_ASSERT(enterFullscreenAction))
            return;

        const auto camera = resourcePool()->getResourceById(
            enterFullscreenAction->cameraId()).dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            return;

        const auto currentLayout = workbench()->currentLayout();
        const auto currentLayoutResource = currentLayout->resource();
        if (!currentLayoutResource
            || !enterFullscreenAction->layoutIds().contains(currentLayoutResource->getId()))
        {
            return;
        }

        auto items = currentLayout->items(camera);
        auto iter = std::find_if(items.begin(), items.end(),
            [](const QnWorkbenchItem* item)
            {
                return item->zoomRect().isNull();
            });

        if (iter != items.end())
        {
            workbench()->setItem(Qn::ZoomedRole, *iter);
            if (enterFullscreenAction->playbackTime() > std::chrono::microseconds::zero())
            {
                const auto navigationTime =
                    qnSyncTime->currentTimePoint() - enterFullscreenAction->playbackTime();

                menu()->triggerIfPossible(
                    nx::vms::client::desktop::ui::action::JumpToTimeAction,
                    nx::vms::client::desktop::ui::action::Parameters().withArgument(
                        Qn::TimestampRole, navigationTime));
            }
        }
    }
    else if (action->type() == utils::type<ExitFullscreenAction>())
    {
        auto exitFullscreenAction = action.dynamicCast<ExitFullscreenAction>();
        if (!NX_ASSERT(exitFullscreenAction))
            return;

        const auto currentLayoutResource = workbench()->currentLayout()->resource();
        if (!currentLayoutResource
            || exitFullscreenAction->layoutIds().contains(currentLayoutResource->getId()))
        {
            return;
        }

        workbench()->setItem(Qn::ZoomedRole, nullptr);
    }
}
