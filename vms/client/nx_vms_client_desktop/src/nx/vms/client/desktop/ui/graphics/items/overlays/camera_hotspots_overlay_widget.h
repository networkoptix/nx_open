// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class CameraHotspotsOverlayWidget: public QnViewportBoundWidget
{
    using base_type = QnViewportBoundWidget;

public:
    CameraHotspotsOverlayWidget(QnMediaResourceWidget* parent);
    virtual ~CameraHotspotsOverlayWidget() override;

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
