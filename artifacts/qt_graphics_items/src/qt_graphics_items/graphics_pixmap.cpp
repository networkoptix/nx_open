// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 1992-2005 Trolltech AS. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "graphics_pixmap.h"

#include <QtGui/QPainter>

#include <QtWidgets/QStyleOptionGraphicsItem>

#include <ui/workaround/sharp_pixmap_painting.h>
#include <nx/vms/client/core/utils/geometry.h>

using nx::vms::client::core::Geometry;

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

