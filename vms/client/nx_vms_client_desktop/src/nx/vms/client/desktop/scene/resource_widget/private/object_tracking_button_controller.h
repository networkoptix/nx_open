// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "button_controller.h"

class GraphicsWidget;
class QnScrollableItemsWidget;
class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class SoftwareTriggerButton;

class ObjectTrackingButtonController: public ButtonController
{
    Q_OBJECT
    using base_type = ButtonController;

public:
    explicit ObjectTrackingButtonController(QnMediaResourceWidget* mediaResourceWidget);
    virtual ~ObjectTrackingButtonController() override;

    virtual void createButtons() override;
    virtual void removeButtons() override;

signals:
    void requestButtonCreation();

protected:
    virtual void handleChangedIOState(const QnIOStateData& value) override;

    virtual void handleApiReply(
        GraphicsWidget* button,
        bool success,
        rest::Handle requestId,
        nx::network::rest::JsonResult& result) override;

signals:
    void ioStateChanged(const QnIOStateData& value);

private:
    bool m_objectTrackingIsActive = false;
};

} // namespace nx::vms::client::desktop
