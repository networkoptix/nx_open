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

#ifndef GRAPHICSFRAME_H
#define GRAPHICSFRAME_H

#include "graphics_widget.h"

class GraphicsFramePrivate;
class GraphicsFrame : public GraphicsWidget
{
    Q_OBJECT
    Q_ENUMS(Shape Shadow)
    Q_PROPERTY(Shape frameShape READ frameShape WRITE setFrameShape)
    Q_PROPERTY(Shadow frameShadow READ frameShadow WRITE setFrameShadow)
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(int midLineWidth READ midLineWidth WRITE setMidLineWidth)
    Q_PROPERTY(int frameWidth READ frameWidth)
    Q_PROPERTY(QRectF frameRect READ frameRect WRITE setFrameRect DESIGNABLE false)

public:
    explicit GraphicsFrame(QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = {});
    virtual ~GraphicsFrame();

    int frameStyle() const;
    void setFrameStyle(int);

    int frameWidth() const;

    enum Shape {
        NoFrame  = 0, // no frame
        Box = 0x0001, // rectangular box
        Panel = 0x0002, // rectangular panel
        WinPanel = 0x0003, // rectangular panel (Windows)
        HLine = 0x0004, // horizontal line
        VLine = 0x0005, // vertical line
        StyledPanel = 0x0006 // rectangular panel depending on the GUI style
    };

    enum Shadow {
        Plain = 0x0010, // plain line
        Raised = 0x0020, // raised shadow effect
        Sunken = 0x0030 // sunken shadow effect
    };

    enum StyleMask {
        Shadow_Mask = 0x00f0, // mask for the shadow
        Shape_Mask = 0x000f // mask for the shape
    };

    Shape frameShape() const;
    void setFrameShape(Shape shape);

    Shadow frameShadow() const;
    void setFrameShadow(Shadow shadow);

    int lineWidth() const;
    void setLineWidth(int width);

    int midLineWidth() const;
    void setMidLineWidth(int width);

    QRectF frameRect() const;
    void setFrameRect(const QRectF &rect);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void initStyleOption(QStyleOption *option) const override;

    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

    virtual bool event(QEvent *event) override;
    virtual void changeEvent(QEvent *event) override;

protected:
    GraphicsFrame(GraphicsFramePrivate& dd, QGraphicsItem* parent, Qt::WindowFlags flags = {});

private:
    Q_DISABLE_COPY(GraphicsFrame)
    Q_DECLARE_PRIVATE(GraphicsFrame)
};

#endif // GRAPHICSFRAME_H
