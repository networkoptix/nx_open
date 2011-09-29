#ifndef VIDEOITEM_H
#define VIDEOITEM_H

#include <QtGui/QGraphicsItem>
#include <QtGui/QGraphicsLayoutItem>

class QPixmap;

class VideoItem : public QGraphicsLayoutItem, public QGraphicsItem
{
public:
    VideoItem(QGraphicsItem *parent = 0);
    ~VideoItem();

    // Inherited from QGraphicsLayoutItem
    void setGeometry(const QRectF &geom);
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    // Inherited from QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
    QPixmap *m_pix;
};

#endif // VIDEOITEM_H
