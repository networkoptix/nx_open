// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <qt_graphics_items/graphics_widget.h>

namespace nx::vms::client::desktop {

class ObjectTrackingButtonPrivate;

class ObjectTrackingButton: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    ObjectTrackingButton(QGraphicsItem* parent = nullptr);
    virtual ~ObjectTrackingButton() override;

signals:
    void clicked();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
