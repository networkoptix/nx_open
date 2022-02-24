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

#ifndef GRAPHICSSLIDER_H
#define GRAPHICSSLIDER_H

#include "abstract_linear_graphics_slider.h"

class GraphicsSliderPrivate;
class GraphicsSliderPositionConverter;

class GraphicsSlider : public AbstractLinearGraphicsSlider
{
    Q_OBJECT

    Q_ENUMS(TickPosition)
    Q_PROPERTY(TickPosition tickPosition READ tickPosition WRITE setTickPosition)
    Q_PROPERTY(qint64 tickInterval READ tickInterval WRITE setTickInterval)

    typedef AbstractLinearGraphicsSlider base_type;

public:
    enum TickPosition {
        NoTicks = 0,
        TicksAbove = 1,
        TicksLeft = TicksAbove,
        TicksBelow = 2,
        TicksRight = TicksBelow,
        TicksBothSides = 3
    };

    explicit GraphicsSlider(QGraphicsItem *parent = 0);
    explicit GraphicsSlider(Qt::Orientation orientation, QGraphicsItem *parent = 0);
    virtual ~GraphicsSlider();

    void setTickPosition(TickPosition position);
    TickPosition tickPosition() const;

    void setTickInterval(qint64 tickInterval);
    qint64 tickInterval() const;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void initStyleOption(QStyleOption *option) const override;

    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

    virtual bool event(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

protected:
    GraphicsSlider(GraphicsSliderPrivate &dd, QGraphicsItem *parent);

    friend class PositionValueConverter;

private:
    Q_DISABLE_COPY(GraphicsSlider)
    Q_DECLARE_PRIVATE(GraphicsSlider)
};


#endif // GRAPHICSSLIDER_H
