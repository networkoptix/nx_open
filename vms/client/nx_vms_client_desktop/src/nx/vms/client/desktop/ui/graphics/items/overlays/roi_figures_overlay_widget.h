// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

#include <nx/utils/impl_ptr.h>
#include <core/resource/resource_fwd.h>

class QnResourceWidget;

namespace nx::vms::client::desktop {

/** Overlay widget which draws ro figures on the scene. */
class RoiFiguresOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    RoiFiguresOverlayWidget(
        const QnVirtualCameraResourcePtr& camera,
        QGraphicsItem* parent = nullptr);
    virtual ~RoiFiguresOverlayWidget() override;

    void setZoomRect(const QRectF& value);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
