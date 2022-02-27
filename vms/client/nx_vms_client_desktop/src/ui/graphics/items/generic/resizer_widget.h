// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

class QnResizerWidget: public GraphicsWidget
{
    using base_type = GraphicsWidget;
public:
    QnResizerWidget(
        Qt::Orientation orientation, QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = {});

    void setEnabled(bool enabled);

    bool isBeingMoved() const;

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    Qt::Orientation m_orientation;
    bool m_isBeingMoved = false;
};
