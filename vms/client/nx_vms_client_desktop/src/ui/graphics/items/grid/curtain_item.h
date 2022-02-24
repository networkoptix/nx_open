// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsObject>

/**
 * Item that fills the whole view with the given color.
 */
class QnCurtainItem: public QGraphicsObject
{
    Q_OBJECT

    typedef QGraphicsObject base_type;

public:
    QnCurtainItem(QGraphicsItem *parent = nullptr);

    virtual QRectF boundingRect() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QColor m_color;
    QRectF m_boundingRect;
};
