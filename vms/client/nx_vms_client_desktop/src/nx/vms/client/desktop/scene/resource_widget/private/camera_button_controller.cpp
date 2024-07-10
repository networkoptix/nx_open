// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_button_controller.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/intercom/utils.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>
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
                menu()->triggerIfPossible(ui::action::SuspendCurrentTourAction);
            });
        connect(m_twoWayAudioWidget, &QnTwoWayAudioWidget::released, this,
            [this]
            {
                menu()->triggerIfPossible(ui::action::ResumeCurrentTourAction);
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
    m_twoWayAudioWidgetId = QnUuid();
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

    return QnDesktopResource::calculateUniqueId(
        systemContext->peerId(),
        workbench()->context()->user()->getId());
}

} // namespace nx::vms::client::desktop
