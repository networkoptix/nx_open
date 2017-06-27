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
    GraphicsWidget *q_ptr;
    GraphicsWidget::HandlingFlags handlingFlags;
    GraphicsWidget::TransformOrigin transformOrigin;
    qreal resizeEffectRadius;
    QPointer<GraphicsWidgetSceneData> sceneData;
    bool inSetGeometry;

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
            constrained(NULL)
        {}
    } *windowData;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};
