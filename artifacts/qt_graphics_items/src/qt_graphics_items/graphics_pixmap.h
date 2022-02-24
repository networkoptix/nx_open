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

#pragma once

#include "graphics_widget.h"

class GraphicsPixmapPrivate;
class GraphicsPixmap : public GraphicsWidget {
    Q_OBJECT

    typedef GraphicsWidget base_type;

public:
    GraphicsPixmap(QGraphicsItem *parent = 0);
    GraphicsPixmap(const QPixmap &pixmap, QGraphicsItem *parent = 0);
    ~GraphicsPixmap();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    QPixmap pixmap() const;
    void setPixmap(const QPixmap &pixmap);

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

private:
    QScopedPointer<GraphicsPixmapPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(GraphicsPixmap)
};
