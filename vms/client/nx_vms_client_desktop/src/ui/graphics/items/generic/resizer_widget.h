#pragma once

#include <ui/graphics/items/standard/graphics_widget.h>

class QnResizerWidget: public GraphicsWidget
{
    using base_type = GraphicsWidget;
public:
    QnResizerWidget(Qt::Orientation orientation, QGraphicsItem*parent = nullptr,
        Qt::WindowFlags wFlags = 0);

    void setEnabled(bool enabled);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    Qt::Orientation m_orientation;
};
