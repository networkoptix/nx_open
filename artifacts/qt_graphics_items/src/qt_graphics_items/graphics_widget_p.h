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

#include <QtCore/QPointer>

#include "graphics_widget.h"

class QStyleOptionTitleBar;

class ConstrainedGeometrically;

class GraphicsWidgetSceneData;

class GraphicsWidgetPrivate {
public:
    GraphicsWidgetPrivate();
    virtual ~GraphicsWidgetPrivate();

    GraphicsWidgetSceneData *ensureSceneData();

    static bool movableAncestorIsSelected(const QGraphicsItem *item);
    bool movableAncestorIsSelected() const { return movableAncestorIsSelected(q_func()); }

    bool hasDecoration() const;
    bool handlesFrameEvents() const;

protected:
    void initStyleOptionTitleBar(QStyleOptionTitleBar *option);

    QRectF mapFromFrame(const QRectF &rect);
    void mapToFrame(QStyleOptionTitleBar *option);

    void ensureWindowData();

    QPointF calculateTransformOrigin() const;

    bool windowFrameMouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    bool windowFrameMousePressEvent(QGraphicsSceneMouseEvent *event);
    bool windowFrameMouseMoveEvent(QGraphicsSceneMouseEvent *event);
    bool windowFrameHoverMoveEvent(QGraphicsSceneHoverEvent *event);
    bool windowFrameHoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    Qn::WindowFrameSections resizingFrameSectionsAt(const QPointF &pos, QWidget *viewport) const;
    Qt::WindowFrameSection resizingFrameSectionAt(const QPointF &pos, QWidget *viewport) const;

protected:
    GraphicsWidget* q_ptr = nullptr;
    GraphicsWidget::HandlingFlags handlingFlags;
    GraphicsWidget::TransformOrigin transformOrigin = GraphicsWidget::Legacy;
    qreal resizeEffectRadius;
    QPointer<GraphicsWidgetSceneData> sceneData;
    bool inSetGeometry = false;

    struct WindowData {
        Qt::WindowFrameSection hoveredSection;
        Qt::WindowFrameSection grabbedSection;
        uint closeButtonHovered: 1;
        uint closeButtonGrabbed: 1;
        QRectF closeButtonRect;
        QPointF startPinPoint;
        QSizeF startSize;
        ConstrainedGeometrically *constrained;
        WindowData():
            grabbedSection(Qt::NoSection),
            closeButtonHovered(false),
            closeButtonGrabbed(false),
            constrained(nullptr)
        {}
    };
    WindowData* windowData = nullptr;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};
