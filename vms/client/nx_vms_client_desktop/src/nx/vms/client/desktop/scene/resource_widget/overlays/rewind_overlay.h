// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnViewportBoundWidget;

namespace nx::vms::client::desktop {

class RewindWidget;
class WindowContext;

class RewindOverlay: public GraphicsWidget, WindowContextAware
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    RewindOverlay(WindowContext* windowContext, QGraphicsItem* parent = nullptr);
    virtual ~RewindOverlay();

    RewindWidget* fastForward() const;
    QnViewportBoundWidget* content() const;

signals:
    void sizeChanged(QSize size);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
