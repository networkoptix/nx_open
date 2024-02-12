// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/api_ioport_data.h>
#include <nx/vms/client/desktop/window_context_aware.h>

#include "button_controller.h"

class QnTwoWayAudioWidget;
class QnMediaResourceWidget;
class IntercomMuteWidget;

namespace nx::vms::client::desktop {

class SoftwareTriggerButton;
class TwoWayAudioManager;

class CameraButtonController: public ButtonController, public WindowContextAware
{
    Q_OBJECT
    using base_type = ButtonController;

    enum class IntercomButtonState
    {
        unmute,
        mute
    };

    struct IntercomMuteButtonData
    {
        SoftwareTriggerButton* button = nullptr;
        nx::Uuid id;
        IntercomButtonState state = IntercomButtonState::unmute;

        std::unique_ptr<TwoWayAudioManager> twoWayAudioManager;
    };

public:
    explicit CameraButtonController(QnMediaResourceWidget* mediaResourceWidget);
    virtual ~CameraButtonController() override;

    virtual void createButtons() override;
    virtual void removeButtons() override;

    void createTwoWayAudioButton();
    void removeTwoWayAudioButton();

    void createIntercomButtons();
    void removeIntercomButtons();

signals:
    void ioStateChanged(const QnIOStateData& value);

protected:
    virtual void handleChangedIOState(const QnIOStateData& value) override;

    virtual void handleApiReply(
        GraphicsWidget* button,
        bool success,
        rest::Handle requestId,
        nx::network::rest::JsonResult& result) override;

private:
    void handleButtonClick(
        SoftwareTriggerButton* button,
        nx::vms::api::ExtendedCameraOutput outputPort);

    QString getDesktopUniqueId() const;

    void createIntercomMuteButton();
    void removeIntercomMuteButton();

    void createOpenDoorButton();
    void removeOpenDoorButton();

private:
    QnTwoWayAudioWidget* m_twoWayAudioWidget = nullptr;
    nx::Uuid m_twoWayAudioWidgetId;

    std::optional<IntercomMuteButtonData> m_intercomMuteButtonData;

    nx::Uuid m_intercomOpenDoorId;
};

} // namespace nx::vms::client::desktop
