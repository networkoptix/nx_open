#include "layoutitem.h"

GraphicsLayoutItem::GraphicsLayoutItem(QGraphicsItem *parent)
    : QGraphicsLayoutItem(), QGraphicsItem(parent)
{
    m_pix = new QPixmap(QLatin1String(":/images/block.png"));
    setGraphicsItem(this);
}

GraphicsLayoutItem::~GraphicsLayoutItem()
{
    delete m_pix;
}

void GraphicsLayoutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    Q_UNUSED(option);

    QRectF frame(QPointF(0,0), geometry().size());
    qreal w = m_pix->width();
    qreal h = m_pix->height();

    // paint a background rect (with gradient)
    QGradientStops stops;
    QLinearGradient gradient(frame.topLeft(), frame.topLeft() + QPointF(200,200));
    stops << QGradientStop(0.0, QColor(60, 60,  60));
    stops << QGradientStop(frame.height()/2/frame.height(), QColor(102, 176, 54));
    //stops << QGradientStop(((frame.height() + h)/2 )/frame.height(), QColor(157, 195,  55));
    stops << QGradientStop(1.0, QColor(215, 215, 215));
    gradient.setStops(stops);
    painter->setBrush(QBrush(gradient));
    painter->drawRoundedRect(frame, 10.0, 10.0);

    // paint a rect around the pixmap (with gradient)
    QPointF pixpos = frame.center() - (QPointF(w, h)/2);
    QRectF innerFrame(pixpos, QSizeF(w, h));
    innerFrame.adjust(-4, -4, +4, +4);
    gradient.setStart(innerFrame.topLeft());
    gradient.setFinalStop(innerFrame.bottomRight());
    stops.clear();
    if(isSelected() || (parentItem() != NULL && parentItem()->isSelected())) { // hack hack hack
        stops << QGradientStop(0.0, QColor(235, 255, 220));
        stops << QGradientStop(0.5, QColor(122, 216, 74));
        stops << QGradientStop(1.0, QColor(20, 20,  20));
    } else {
        stops << QGradientStop(0.0, QColor(215, 255, 200));
        stops << QGradientStop(0.5, QColor(102, 176, 54));
        stops << QGradientStop(1.0, QColor(0, 0,  0));
    }
    gradient.setStops(stops);
    painter->setBrush(QBrush(gradient));
    painter->drawRoundedRect(innerFrame, 10.0, 10.0);
    painter->drawPixmap(pixpos, *m_pix);
}

QRectF GraphicsLayoutItem::boundingRect() const
{
    return QRectF(QPointF(0,0), geometry().size());
}

void GraphicsLayoutItem::setGeometry(const QRectF &geom)
{
    prepareGeometryChange();
    QGraphicsLayoutItem::setGeometry(geom);
    setPos(geom.topLeft());
}

QSizeF GraphicsLayoutItem::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    switch (which) {
    case Qt::MinimumSize:
    case Qt::PreferredSize:
        // Do not allow a size smaller than the pixmap with two frames around it.
        return m_pix->size() + QSize(12, 12);
    case Qt::MaximumSize:
        return QSizeF(5000, 5000);//sizeHint(Qt::PreferredSize, constraint) * 3;
    default:
        break;
    }
    return constraint;
}
