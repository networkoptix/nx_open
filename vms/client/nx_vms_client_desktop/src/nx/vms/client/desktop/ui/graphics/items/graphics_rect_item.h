#pragma once

#include <QtWidgets/QGraphicsRectItem>

namespace nx::vms::client::desktop {

class GraphicsRectItem: public QGraphicsRectItem
{
    using base_type = QGraphicsRectItem;

public:
    GraphicsRectItem(QGraphicsItem* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* w) override;
};

} // namespace nx::vms::client::desktop
