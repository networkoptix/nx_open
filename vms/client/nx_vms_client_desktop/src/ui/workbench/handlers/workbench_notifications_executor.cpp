// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_notifications_executor.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/audio/audiodevice.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/actions/enter_fullscreen_action.h>
#include <nx/vms/rules/actions/exit_fullscreen_action.h>
#include <nx/vms/rules/actions/open_layout_action.h>
#include <nx/vms/rules/actions/play_sound_action.h>
#include <nx/vms/rules/actions/ptz_preset_action.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/synctime.h>
#include <utils/media/audio_player.h>

using namespace nx::vms::client::desktop;
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

    const auto userGroups = nx::vms::common::userGroupsWithParents(user);
    return userGroups.intersects(userSelection.ids);
}

} // namespace

QnWorkbenchNotificationsExecutor::QnWorkbenchNotificationsExecutor(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    auto engine = systemContext()->vmsRulesEngine();

    engine->addActionExecutor(utils::type<EnterFullscreenAction>(), this);
    engine->addActionExecutor(utils::type<ExitFullscreenAction>(), this);
    engine->addActionExecutor(utils::type<NotificationAction>(), this);
    engine->addActionExecutor(utils::type<OpenLayoutAction>(), this);
    engine->addActionExecutor(utils::type<PlaySoundAction>(), this);
    engine->addActionExecutor(utils::type<PtzPresetAction>(), this);
    engine->addActionExecutor(utils::type<RepeatSoundAction>(), this);
    engine->addActionExecutor(utils::type<ShowOnAlarmLayoutAction>(), this);
    engine->addActionExecutor(utils::type<SpeakAction>(), this);
}

void QnWorkbenchNotificationsExecutor::execute(const ActionPtr& action)
{
    if (!checkUserPermissions(context()->user(), action))
        return;

    const auto& actionType = action->type();

    if (actionType == utils::type<NotificationAction>()
        || actionType == utils::type<RepeatSoundAction>()
        || actionType == utils::type<ShowOnAlarmLayoutAction>())
    {
        auto notificationAction = action.dynamicCast<NotificationActionBase>();
        if (!NX_ASSERT(notificationAction, "Unexpected action: %1", actionType))
            return;

        emit notificationActionReceived(notificationAction);
    }
    else if (action->type() == utils::type<PlaySoundAction>())
    {
        auto soundAction = action.dynamicCast<PlaySoundAction>();
        if (!NX_ASSERT(soundAction))
            return;

        QString filePath = context()->instance<ServerNotificationCache>()->getFullPath(
            soundAction->sound());
        // If file doesn't exist then it's already deleted or not downloaded yet.
        // It should not be played when downloaded.
        AudioPlayer::playFileAsync(filePath);
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
                    ui::action::JumpToTimeAction,
                    ui::action::Parameters().withArgument(
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
    else if (action->type() == utils::type<PtzPresetAction>())
    {
        auto ptzAction = action.dynamicCast<PtzPresetAction>();
        if (!NX_ASSERT(ptzAction))
            return;

        auto resource = resourcePool()->getResourceById(ptzAction->cameraId());
        if (!resource)
            return;

        for (auto widget: display()->widgets(resource))
        {
            auto mediaResourceWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
            // Server processes PTZ actions for the real cameras,
            // so client receives only Fisheye presets.
            if (!mediaResourceWidget || !mediaResourceWidget->dewarpingParams().enabled)
                return;

            if (auto controller = mediaResourceWidget->ptzController())
            {
                controller->activatePreset(
                    ptzAction->presetId(),
                    QnAbstractPtzController::MaxPtzSpeed);
            }
        }
    }
    else if (action->type() == utils::type<OpenLayoutAction>())
    {
        auto layoutAction = action.dynamicCast<OpenLayoutAction>();
        if (!NX_ASSERT(layoutAction))
            return;

        const auto layout = resourcePool()->getResourceById<LayoutResource>(
            layoutAction->layoutId());
        if (!layout)
            return;

        menu()->trigger(ui::action::OpenInNewTabAction, layout);
        if (layoutAction->playbackTime().count() > 0)
        {
            const auto navigationTime = qnSyncTime->currentTimePoint()
                - layoutAction->playbackTime();

            menu()->triggerIfPossible(ui::action::JumpToTimeAction,
                ui::action::Parameters().withArgument(Qn::TimestampRole, navigationTime));
        }
    }
}
