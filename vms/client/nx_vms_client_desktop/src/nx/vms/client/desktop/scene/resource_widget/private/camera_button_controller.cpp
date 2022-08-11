// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_button_controller.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/software_trigger_button.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kTriggerButtonHeight = 40;

} // namespace

CameraButtonController::CameraButtonController(QnMediaResourceWidget* mediaResourceWidget):
    base_type(mediaResourceWidget),
    WindowContextAware(mediaResourceWidget->windowContext())
{
}

void CameraButtonController::createButtons()
{
    if (m_camera.isNull())
        return;

    ensureButtonState(ExtendedCameraOutput::heater,
        [this]
        {
            auto heaterButton = new SoftwareTriggerButton(m_parentWidget);
            heaterButton->setIcon("heater");
            heaterButton->setToolTip(tr("Heater"));

            connect(heaterButton, &SoftwareTriggerButton::clicked,
                [this, heaterButton]
                {
                    if (!heaterButton->isLive())
                    {
                        menu()->trigger(ui::action::JumpToLiveAction, m_parentWidget);
                        return;
                    }

                    heaterButton->setState(SoftwareTriggerButton::State::Waiting);
                    buttonClickHandler(
                        heaterButton,
                        ExtendedCameraOutput::heater,
                        nx::vms::api::EventState::active);
                });

            return heaterButton;
        });

    ensureButtonState(ExtendedCameraOutput::wiper,
        [this]
        {
            auto wiperButton = new SoftwareTriggerButton(m_parentWidget);
            wiperButton->setIcon("wiper");
            wiperButton->setToolTip(tr("Wiper"));

            connect(wiperButton, &SoftwareTriggerButton::clicked,
                [this, wiperButton]
                {
                    if (!wiperButton->isLive())
                    {
                        menu()->trigger(ui::action::JumpToLiveAction, m_parentWidget);
                        return;
                    }

                    wiperButton->setState(SoftwareTriggerButton::State::Waiting);
                    buttonClickHandler(
                        wiperButton,
                        ExtendedCameraOutput::wiper,
                        nx::vms::api::EventState::active);
                });

            return wiperButton;
        });
}

void CameraButtonController::removeButtons()
{
    ButtonController::removeButtons();
}

void CameraButtonController::createTwoAudioButton()
{
    if (!m_camera)
        return;

    if (!m_twoWayAudioWidget)
    {
        auto systemContext = SystemContext::fromResource(m_camera);
        const QString desktopResourceUniqueId = core::DesktopResource::calculateUniqueId(
            systemContext->peerId(),
            workbench()->context()->user()->getId());
        m_twoWayAudioWidget =
            new QnTwoWayAudioWidget(desktopResourceUniqueId, m_parentWidget);
        m_twoWayAudioWidget->setCamera(m_camera->audioOutputDevice());
        m_twoWayAudioWidget->setFixedHeight(kTriggerButtonHeight);
        statisticsModule()->controls()->registerButton(
            "two_way_audio",
            m_twoWayAudioWidget);

        connect(m_twoWayAudioWidget, &QnTwoWayAudioWidget::pressed, this,
            [this]
            {
                menu()->triggerIfPossible(ui::action::SuspendCurrentTourAction);
            });
        connect(m_twoWayAudioWidget, &QnTwoWayAudioWidget::released, this,
            [this]
            {
                menu()->triggerIfPossible(ui::action::ResumeCurrentTourAction);
            });

        m_twoWayAudioWidgetId = m_buttonsContainer->addItem(m_twoWayAudioWidget);
    }
    else
    {
        m_twoWayAudioWidget->setCamera(m_camera->audioOutputDevice());
    }
}

void CameraButtonController::removeTwoAudioButton()
{
    if (m_twoWayAudioWidgetId.isNull())
        return;

    m_buttonsContainer->deleteItem(m_twoWayAudioWidgetId);
    m_twoWayAudioWidget = nullptr;
    m_twoWayAudioWidgetId = QnUuid();
}

void CameraButtonController::handleChangedIOState(const QnIOStateData& value)
{
    // Do nothing.
}

void CameraButtonController::handleApiReply(
    GraphicsWidget* button,
    bool success,
    rest::Handle /*requestId*/,
    nx::network::rest::JsonResult& /*result*/)
{
    auto triggerButton = dynamic_cast<SoftwareTriggerButton*>(button);

    if (!triggerButton)
        return;

    triggerButton->setState(success
        ? SoftwareTriggerButton::State::Success
        : SoftwareTriggerButton::State::Failure);
}

} // namespace nx::vms::client::desktop
