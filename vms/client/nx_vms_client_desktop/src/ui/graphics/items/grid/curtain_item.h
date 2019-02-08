#pragma once

#include <QtWidgets/QGraphicsObject>

#include <ui/customization/customized.h>

/**
 * Item that fills the whole view with the given color.
 */
class QnCurtainItem: public Customized<QGraphicsObject>
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)

    typedef Customized<QGraphicsObject> base_type;
public:
    QnCurtainItem(QGraphicsItem *parent = NULL);

    virtual QRectF boundingRect() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const QColor &color() const;
    void setColor(const QColor &color);



private:
    QColor m_color;
    QRectF m_boundingRect;
};
