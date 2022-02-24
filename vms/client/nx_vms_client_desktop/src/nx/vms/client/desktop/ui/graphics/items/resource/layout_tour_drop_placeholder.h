// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/graphics/items/overlays/overlayed.h>
#include <qt_graphics_items/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>

class QnViewportBoundWidget;

namespace nx::vms::client::desktop {
namespace ui {

class LayoutTourDropPlaceholder: public Overlayed<QnFramedWidget>
{
    Q_OBJECT
    using base_type = Overlayed<QnFramedWidget>;

public:
    LayoutTourDropPlaceholder(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {});

    const QRectF& rect() const;
    void setRect(const QRectF& rect);

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    QRectF m_rect;
    QnViewportBoundWidget* m_widget;
};

} // namespace ui
} // namespace nx::vms::client::desktop
