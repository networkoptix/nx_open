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


#include "graphics_path_item.h"

#include <QtGui/QPainter>


GraphicsPathItem::GraphicsPathItem(QGraphicsItem *parent):
    base_type(parent)
{}

GraphicsPathItem::~GraphicsPathItem() {
    return;
}

QPainterPath GraphicsPathItem::path() const {
    return m_path;
}

void GraphicsPathItem::setPath(const QPainterPath &path) {
    if (m_path == path)
        return;
    invalidateBoundingRect();
    m_path = path;
    update();
}

QRectF GraphicsPathItem::calculateBoundingRect() const {
    qreal penWidth = pen().widthF();
    if (qFuzzyIsNull(penWidth))
        return m_path.controlPointRect();
    else {
        return shape().controlPointRect();
    }
}

QPainterPath GraphicsPathItem::shape() const {
    return shapeFromPath(m_path, pen());
}

void GraphicsPathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawPath(m_path);
}
