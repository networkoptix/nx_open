// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_action_executor.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/audio/audiodevice.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/actions/enter_fullscreen_action.h>
#include <nx/vms/rules/actions/exit_fullscreen_action.h>
#include <nx/vms/rules/actions/open_layout_action.h>
#include <nx/vms/rules/actions/play_sound_action.h>
#include <nx/vms/rules/actions/ptz_preset_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/synctime.h>
#include <utils/media/audio_player.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::rules;

WorkbenchActionExecutor::WorkbenchActionExecutor(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    auto engine = systemContext()->vmsRulesEngine();

    engine->addActionExecutor(rules::utils::type<EnterFullscreenAction>(), this);
    engine->addActionExecutor(rules::utils::type<ExitFullscreenAction>(), this);
    engine->addActionExecutor(rules::utils::type<OpenLayoutAction>(), this);
    engine->addActionExecutor(rules::utils::type<PlaySoundAction>(), this);
    engine->addActionExecutor(rules::utils::type<PtzPresetAction>(), this);
    engine->addActionExecutor(rules::utils::type<SpeakAction>(), this);

}

WorkbenchActionExecutor::~WorkbenchActionExecutor()
{}

void WorkbenchActionExecutor::execute(const ActionPtr& action)
{
    if (!checkUserPermissions(context()->user(), action))
        return;

    const auto& actionType = action->type();

    if (action->type() == rules::utils::type<PlaySoundAction>())
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
    else if (action->type() == rules::utils::type<SpeakAction>())
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
    else if (action->type() == rules::utils::type<EnterFullscreenAction>())
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
                    menu::JumpToTimeAction,
                    menu::Parameters().withArgument(
                        Qn::TimestampRole, navigationTime));
            }
        }
    }
    else if (action->type() == rules::utils::type<ExitFullscreenAction>())
    {
        auto exitFullscreenAction = action.dynamicCast<ExitFullscreenAction>();
        if (!NX_ASSERT(exitFullscreenAction))
            return;

        if (const auto currentLayoutResource = workbench()->currentLayout()->resource();
            currentLayoutResource
            && exitFullscreenAction->layoutIds().contains(currentLayoutResource->getId()))
        {
            workbench()->setItem(Qn::ZoomedRole, nullptr);
        }
    }
    else if (action->type() == rules::utils::type<PtzPresetAction>())
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
    else if (action->type() == rules::utils::type<OpenLayoutAction>())
    {
        auto layoutAction = action.dynamicCast<OpenLayoutAction>();
        if (!NX_ASSERT(layoutAction))
            return;

        const auto layout = resourcePool()->getResourceById<LayoutResource>(
            layoutAction->layoutId());
        if (!layout)
            return;

        menu()->trigger(menu::OpenInNewTabAction, layout);
        if (layoutAction->playbackTime().count() > 0)
        {
            const auto navigationTime = qnSyncTime->currentTimePoint()
                - layoutAction->playbackTime();

            menu()->triggerIfPossible(menu::JumpToTimeAction,
                menu::Parameters().withArgument(Qn::TimestampRole, navigationTime));
        }
    }
    else
    {
        NX_ASSERT(false, "Unexpected action: %1", actionType);
    }
}

} // namespace nx::vms::client::desktop
