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
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/intercom/utils.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/software_trigger_button.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

#include "two_way_audio_manager.h"

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kTriggerButtonHeight = 40;

void setMuteState(SoftwareTriggerButton* button)
{
    button->setIcon("mute_call");
    button->setToolTip(CameraButtonController::tr("Mute"));
}

void setUnmuteStata(SoftwareTriggerButton* button)
{
    button->setIcon("unmute_call");
    button->setToolTip(CameraButtonController::tr("Unmute"));
}

} // namespace

CameraButtonController::CameraButtonController(QnMediaResourceWidget* mediaResourceWidget):
    base_type(mediaResourceWidget),
    WindowContextAware(mediaResourceWidget->windowContext())
{
}

CameraButtonController::~CameraButtonController()
{
    if (m_intercomMuteButtonData && m_intercomMuteButtonData->twoWayAudioManager)
        m_intercomMuteButtonData->twoWayAudioManager->disconnect();
}

void CameraButtonController::createButtons()
{
    if (!m_camera)
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
                    handleButtonClick(heaterButton, ExtendedCameraOutput::heater);
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
                    handleButtonClick(wiperButton, ExtendedCameraOutput::wiper);
                });

            return wiperButton;
        });
}

void CameraButtonController::removeButtons()
{
    removeButton(ExtendedCameraOutput::heater);
    removeButton(ExtendedCameraOutput::wiper);
}

void CameraButtonController::createTwoWayAudioButton()
{
    if (!m_camera)
        return;

    if (!m_twoWayAudioWidget)
    {
        m_twoWayAudioWidget = new QnTwoWayAudioWidget(
            getDesktopUniqueId(),
            m_camera,
            m_parentWidget);
        m_twoWayAudioWidget->setFixedHeight(kTriggerButtonHeight);
        statisticsModule()->controls()->registerButton(
            "two_way_audio",
            m_twoWayAudioWidget);

        connect(m_twoWayAudioWidget, &QnTwoWayAudioWidget::pressed, this,
            [this]
            {
                menu()->triggerIfPossible(ui::action::SuspendCurrentShowreelAction);
            });
        connect(m_twoWayAudioWidget, &QnTwoWayAudioWidget::released, this,
            [this]
            {
                menu()->triggerIfPossible(ui::action::ResumeCurrentShowreelAction);
            });

        m_twoWayAudioWidgetId = m_buttonsContainer->addItem(m_twoWayAudioWidget);
    }
}

void CameraButtonController::removeTwoWayAudioButton()
{
    if (m_twoWayAudioWidgetId.isNull())
        return;

    m_buttonsContainer->deleteItem(m_twoWayAudioWidgetId);
    m_twoWayAudioWidget = nullptr;
    m_twoWayAudioWidgetId = nx::Uuid();
}

void CameraButtonController::createIntercomButtons()
{
    createIntercomMuteButton();
    createOpenDoorButton();
}

void CameraButtonController::removeIntercomButtons()
{
    removeIntercomMuteButton();
    removeOpenDoorButton();
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

void CameraButtonController::handleButtonClick(
    SoftwareTriggerButton* button,
    ExtendedCameraOutput outputPort)
{
    if (!button->isLive())
    {
        menu()->trigger(ui::action::JumpToLiveAction, m_parentWidget);
        return;
    }

    button->setState(SoftwareTriggerButton::State::Waiting);
    buttonClickHandler(button, outputPort, nx::vms::api::EventState::active);
}

QString CameraButtonController::getDesktopUniqueId() const
{
    if (!NX_ASSERT(m_camera))
        return "";

    const auto systemContext = SystemContext::fromResource(m_camera);
    if (!NX_ASSERT(systemContext))
        return "";

    if (!NX_ASSERT(workbench()->context()->user()))
        return "";

    return core::DesktopResource::calculateUniqueId(
        systemContext->peerId(),
        workbench()->context()->user()->getId());
}

void CameraButtonController::createIntercomMuteButton()
{
    if (!m_camera)
        return;

    if (m_intercomMuteButtonData)
        return;

    m_intercomMuteButtonData.emplace(IntercomMuteButtonData());

    m_intercomMuteButtonData->button = new SoftwareTriggerButton(m_parentWidget);
    m_intercomMuteButtonData->id = m_buttonsContainer->addItem(m_intercomMuteButtonData->button);
    m_intercomMuteButtonData->twoWayAudioManager.reset(
        new TwoWayAudioManager(getDesktopUniqueId(), m_camera));

    connect(m_intercomMuteButtonData->button, &SoftwareTriggerButton::clicked,
        [this]
        {
            if (!m_intercomMuteButtonData->button->isLive())
            {
                menu()->trigger(ui::action::JumpToLiveAction, m_parentWidget);
                return;
            }

            if (m_intercomMuteButtonData->state == IntercomButtonState::unmute)
                m_intercomMuteButtonData->twoWayAudioManager->startStreaming();
            else
                m_intercomMuteButtonData->twoWayAudioManager->stopStreaming();
        });

    connect(m_intercomMuteButtonData->twoWayAudioManager.get(),
        &TwoWayAudioManager::streamingStateChanged,
        [this](TwoWayAudioManager::StreamingState state)
        {
            if (state == TwoWayAudioManager::StreamingState::active)
            {
                setMuteState(m_intercomMuteButtonData->button);
            }
            else if (state == TwoWayAudioManager::StreamingState::disabled
                || (state == TwoWayAudioManager::StreamingState::error
                    && m_intercomMuteButtonData->state == IntercomButtonState::mute))
            {
                setUnmuteStata(m_intercomMuteButtonData->button);
            }

            m_intercomMuteButtonData->state = (state == TwoWayAudioManager::StreamingState::active)
                ? IntercomButtonState::mute
                : IntercomButtonState::unmute;

            if (state == TwoWayAudioManager::StreamingState::error)
                m_intercomMuteButtonData->button->setState(SoftwareTriggerButton::State::Failure);
        });

    NX_ASSERT(m_intercomMuteButtonData->state == IntercomButtonState::unmute);
    setUnmuteStata(m_intercomMuteButtonData->button);
}

void CameraButtonController::removeIntercomMuteButton()
{
    if (m_intercomMuteButtonData)
        m_buttonsContainer->deleteItem(m_intercomMuteButtonData->id);

    m_intercomMuteButtonData.reset();

}

void CameraButtonController::createOpenDoorButton()
{
    if (!m_camera)
        return;

    ensureButtonState(ExtendedCameraOutput::powerRelay,
        [this]
        {
            auto openDoorButton = new SoftwareTriggerButton(m_parentWidget);
            openDoorButton->setIcon("_door_opened");
            openDoorButton->setToolTip(tr("Open Door"));

            connect(openDoorButton, &SoftwareTriggerButton::clicked,
                [this, openDoorButton]
                {
                    handleButtonClick(openDoorButton, ExtendedCameraOutput::powerRelay);
                });

            return openDoorButton;
        });
}

void CameraButtonController::removeOpenDoorButton()
{
    removeButton(ExtendedCameraOutput::powerRelay);
}

} // namespace nx::vms::client::desktop
