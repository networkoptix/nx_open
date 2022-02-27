// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PARTICLE_ITEM_H
#define QN_PARTICLE_ITEM_H

#include <QtWidgets/QGraphicsObject>
#include <QtGui/QBrush>

/**
 * A radial gradient particle item.
 */
class QnParticleItem: public QGraphicsObject {
    Q_OBJECT;
    typedef QGraphicsObject base_type;

public:
    QnParticleItem(QGraphicsItem *parent = nullptr);
    virtual ~QnParticleItem();

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color);

    virtual QRectF boundingRect() const override {
        return rect();
    }

    const QRectF &rect() const {
        return m_rect;
    }

    void setRect(const QRectF &rect);

    void setPaintOpacity(qreal opacity);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF m_rect;
    QColor m_color;
    QBrush m_brush;
    qreal m_paintOpacity = 0.0;
};


#endif // QN_PARTICLE_ITEM_H
