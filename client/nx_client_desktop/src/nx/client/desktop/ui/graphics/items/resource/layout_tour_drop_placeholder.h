#pragma once

#include <ui/graphics/items/standard/graphics_widget.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class LayoutTourDropPlaceholder: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;
public:
    LayoutTourDropPlaceholder(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = 0);

    virtual QRectF boundingRect() const override;
    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    const QRectF& rect() const;
    void setRect(const QRectF& rect);

private:
    QRectF m_rect;

};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
