#pragma once

#include <ui/graphics/items/overlays/overlayed.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>

class QnViewportBoundWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class LayoutTourDropPlaceholder: public Overlayed<QnFramedWidget>
{
    Q_OBJECT
    using base_type = Overlayed<QnFramedWidget>;

public:
    LayoutTourDropPlaceholder(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = 0);

    const QRectF& rect() const;
    void setRect(const QRectF& rect);

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    QRectF m_rect;
    QnViewportBoundWidget* m_widget;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
