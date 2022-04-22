// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "button_controller.h"

class QnTwoWayAudioWidget;
class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class CameraButtonController: public ButtonController
{
    Q_OBJECT
    using base_type = ButtonController;

public:
    explicit CameraButtonController(QnMediaResourceWidget* mediaResourceWidget);

    virtual void createButtons() override;
    virtual void removeButtons() override;

    void createTwoAudioButton();
    void removeTwoAudioButton();

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
    QnTwoWayAudioWidget* m_twoWayAudioWidget = nullptr;
    QnUuid m_twoWayAudioWidgetId;
};

} // namespace nx::vms::client::desktop
