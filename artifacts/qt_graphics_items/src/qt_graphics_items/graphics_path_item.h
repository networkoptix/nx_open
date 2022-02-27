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

#ifndef QN_GRAPHICS_PATH_ITEM_H
#define QN_GRAPHICS_PATH_ITEM_H

#include "abstract_graphics_shape_item.h"

/**
 * Item that draws a given painter path. Like a <tt>QGraphicsPathItem</tt>, but is a 
 * <tt>QObject</tt>.
 */
class GraphicsPathItem: public AbstractGraphicsShapeItem {
    Q_OBJECT
    typedef AbstractGraphicsShapeItem base_type;

public:
    GraphicsPathItem(QGraphicsItem *parent = nullptr);
    virtual ~GraphicsPathItem();

    QPainterPath path() const;
    void setPath(const QPainterPath &path);

    virtual QPainterPath shape() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual QRectF calculateBoundingRect() const override;

private:
    QPainterPath m_path;
};

#endif // QN_GRAPHICS_PATH_ITEM_H
