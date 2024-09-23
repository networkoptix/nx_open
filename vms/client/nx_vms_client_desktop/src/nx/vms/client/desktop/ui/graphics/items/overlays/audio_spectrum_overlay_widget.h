// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/vms/client/desktop/camera/camera_fwd.h>

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class AudioSpectrumWidget;

class AudioSpectrumOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    AudioSpectrumOverlayWidget(QnResourceDisplayPtr display, QGraphicsWidget* parent);
    virtual ~AudioSpectrumOverlayWidget() override;

    AudioSpectrumWidget* audioSpectrumWidget() const;

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
