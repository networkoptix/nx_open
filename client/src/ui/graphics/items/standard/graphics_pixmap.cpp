#include "graphics_pixmap.h"

#include <ui/common/geometry.h>

class GraphicsPixmapPrivate {
public:
    QPixmap pixmap;
    Qt::AspectRatioMode aspectRatioMode;

    GraphicsPixmapPrivate()
        : aspectRatioMode(Qt::KeepAspectRatio)
    {}
};

GraphicsPixmap::GraphicsPixmap(QGraphicsItem *parent)
    : base_type(parent)
    , d_ptr(new GraphicsPixmapPrivate())
{

}

GraphicsPixmap::GraphicsPixmap(const QPixmap &pixmap, QGraphicsItem *parent)
    : base_type(parent)
    , d_ptr(new GraphicsPixmapPrivate())
{
    Q_D(GraphicsPixmap);

    d->pixmap = pixmap;
}

GraphicsPixmap::~GraphicsPixmap() {}

void GraphicsPixmap::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)
    Q_D(GraphicsPixmap);

    if (d->pixmap.isNull())
        return;

    QRectF rect = d->pixmap.rect();
    rect = QnGeometry::scaled(rect, option->rect.size(), rect.center(), d->aspectRatioMode);
    rect.moveCenter(option->rect.center());

    painter->drawPixmap(rect.toRect(), d->pixmap);
}

Qt::AspectRatioMode GraphicsPixmap::aspectRatioMode() const {
    Q_D(GraphicsPixmap);
    return d->aspectRatioMode;
}

void GraphicsPixmap::setAspectRatioMode(Qt::AspectRatioMode mode) {
    Q_D(GraphicsPixmap);

    if (d->aspectRatioMode == mode)
        return;

    d->aspectRatioMode = mode;
    update();
}

QPixmap GraphicsPixmap::pixmap() const {
    Q_D(GraphicsPixmap);
    return d->pixmap;
}

void GraphicsPixmap::setPixmap(const QPixmap &pixmap) {
    Q_D(GraphicsPixmap);
    d->pixmap = pixmap;
    update();
}

QSizeF GraphicsPixmap::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    Q_D(const GraphicsPixmap);

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

