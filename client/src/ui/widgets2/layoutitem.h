#ifndef LAYOUTITEM_H
#define LAYOUTITEM_H

#include <QtGui>

class GraphicsLayoutItem : public QGraphicsLayoutItem, public QGraphicsItem
{
public:
    GraphicsLayoutItem(QGraphicsItem *parent = 0);
    ~GraphicsLayoutItem();

    // Inherited from QGraphicsLayoutItem
    void setGeometry(const QRectF &geom);
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    // Inherited from QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
    QPixmap *m_pix;
};

#endif // LAYOUTITEM_H
