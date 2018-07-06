#pragma once

#include <QtWidgets/QGraphicsRectItem>

namespace nx {
namespace client {
namespace desktop {

class GraphicsRectItem: public QGraphicsRectItem
{
    using base_type = QGraphicsRectItem;

public:
    GraphicsRectItem(QGraphicsItem* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* w) override;
};

} // namespace desktop
} // namespace client
} // namespace nx
