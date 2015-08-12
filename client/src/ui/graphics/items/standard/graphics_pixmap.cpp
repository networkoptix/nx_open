#include "graphics_pixmap.h"

#include <ui/common/geometry.h>

class GraphicsPixmapPrivate : public QObject {
public:
    GraphicsPixmap *widget;
    QPixmap pixmap;
    Qt::AspectRatioMode aspectRatioMode;

    GraphicsPixmapPrivate(GraphicsPixmap *parent)
        : QObject(parent)
        , widget(parent)
        , aspectRatioMode(Qt::KeepAspectRatio)
    {}
};

GraphicsPixmap::GraphicsPixmap(QGraphicsItem *parent)
    : base_type(parent)
    , d(new GraphicsPixmapPrivate(this))
{

}

GraphicsPixmap::GraphicsPixmap(const QPixmap &pixmap, QGraphicsItem *parent)
    : base_type(parent)
    , d(new GraphicsPixmapPrivate(this))
{
    d->pixmap = pixmap;
}

GraphicsPixmap::~GraphicsPixmap() {}

void GraphicsPixmap::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)

    if (d->pixmap.isNull())
        return;

    QRectF rect = d->pixmap.rect();
    rect = QnGeometry::scaled(rect, option->rect.size(), rect.center(), d->aspectRatioMode);
    rect.moveCenter(option->rect.center());

    painter->drawPixmap(rect.toRect(), d->pixmap);
}

Qt::AspectRatioMode GraphicsPixmap::aspectRatioMode() const {
    return d->aspectRatioMode;
}

void GraphicsPixmap::setAspectRatioMode(Qt::AspectRatioMode mode) {
    if (d->aspectRatioMode == mode)
        return;

    d->aspectRatioMode = mode;
    update();
}

QPixmap GraphicsPixmap::pixmap() const {
    return d->pixmap;
}

void GraphicsPixmap::setPixmap(const QPixmap &pixmap) {
    d->pixmap = pixmap;
    update();
}

QSizeF GraphicsPixmap::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if (!d->pixmap.isNull()) {
        switch (which) {
        case Qt::PreferredSize:
        case Qt::MinimumSize:
            return d->pixmap.size();
        default:
            break;
        }
    }

    return base_type::sizeHint(which, constraint);
}

