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

#ifndef GRAPHICSSCROLLBAR_H
#define GRAPHICSSCROLLBAR_H

#include "abstract_linear_graphics_slider.h"

class GraphicsScrollBarPrivate;

class GraphicsScrollBar : public AbstractLinearGraphicsSlider {
    Q_OBJECT;

    typedef AbstractLinearGraphicsSlider base_type;

public:
    explicit GraphicsScrollBar(QGraphicsItem *parent = 0);
    explicit GraphicsScrollBar(Qt::Orientation, QGraphicsItem *parent = 0);
    virtual ~GraphicsScrollBar();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;

protected:
    GraphicsScrollBar(GraphicsScrollBarPrivate &dd, Qt::Orientation orientation, QGraphicsItem *parent);

    virtual bool sceneEvent(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    virtual void hideEvent(QHideEvent *) override;
#ifndef QT_NO_CONTEXTMENU
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *) override;
#endif
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    virtual void sliderChange(SliderChange change) override;
    virtual void initStyleOption(QStyleOption *option) const override;

private:
    Q_DISABLE_COPY(GraphicsScrollBar)
    Q_DECLARE_PRIVATE(GraphicsScrollBar)
};

#endif // GRAPHICSSCROLLBAR_H
