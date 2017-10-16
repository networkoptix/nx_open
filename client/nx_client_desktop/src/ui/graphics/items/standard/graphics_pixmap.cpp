#include "graphics_pixmap.h"

#include <QtGui/QPainter>

#include <QtWidgets/QStyleOptionGraphicsItem>

#include <ui/workaround/sharp_pixmap_painting.h>
#include <nx/client/core/utils/geometry.h>

using nx::client::core::utils::Geometry;

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
    rect = Geometry::scaled(rect, option->rect.size(), rect.center(), d->aspectRatioMode);
    rect.moveCenter(option->rect.center());
    paintPixmapSharp(painter, d->pixmap, rect.toRect());
}

Qt::AspectRatioMode GraphicsPixmap::aspectRatioMode() const {
    Q_D(const GraphicsPixmap);
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
    Q_D(const GraphicsPixmap);
    return d->pixmap;
}

void GraphicsPixmap::setPixmap(const QPixmap &pixmap) {
    Q_D(GraphicsPixmap);
    d->pixmap = pixmap;
    update();
}

QSizeF GraphicsPixmap::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    Q_D(const GraphicsPixmap);

    if (!d->pixmap.isNull())
    {
        switch (which)
        {
            case Qt::PreferredSize:
            case Qt::MinimumSize:
                return d->pixmap.size() / d->pixmap.devicePixelRatio();
            default:
                break;
        }
    }

    return base_type::sizeHint(which, constraint);
}

