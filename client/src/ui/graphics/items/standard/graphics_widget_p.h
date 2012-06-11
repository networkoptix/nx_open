#ifndef QN_GRAPHICS_WIDGET_P_H
#define QN_GRAPHICS_WIDGET_P_H

#include "graphics_widget.h"

class QStyleOptionTitleBar;

class GraphicsStyle;
class GraphicsWidgetSceneData;

class GraphicsWidgetPrivate {
public:
    GraphicsWidgetPrivate(): q_ptr(NULL), handlingFlags(0), style(NULL), windowData(NULL) {};
    virtual ~GraphicsWidgetPrivate();

    GraphicsWidgetSceneData *ensureSceneData();
    
    static bool movableAncestorIsSelected(const QGraphicsItem *item);
    bool movableAncestorIsSelected() const { return movableAncestorIsSelected(q_func()); }

    bool hasDecoration() const;

protected:
    void initStyleOptionTitleBar(QStyleOptionTitleBar *option);

    QRectF mapFromFrame(const QRectF &rect);
    void mapToFrame(QStyleOptionTitleBar *option);

    void ensureWindowData();

    void windowFrameMouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void windowFrameMousePressEvent(QGraphicsSceneMouseEvent *event);
    void windowFrameMouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void windowFrameHoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void windowFrameHoverLeaveEvent(QGraphicsSceneHoverEvent *event);

protected:
    GraphicsWidget *q_ptr;
    GraphicsWidget::HandlingFlags handlingFlags;
    QWeakPointer<GraphicsWidgetSceneData> sceneData;
    mutable GraphicsStyle *style;
    mutable QScopedPointer<GraphicsStyle> reserveStyle;

    struct WindowData {
        Qt::WindowFrameSection hoveredSection;
        Qt::WindowFrameSection grabbedSection;
        uint closeButtonHovered: 1;
        uint closeButtonGrabbed: 1;
        QRectF closeButtonRect;
        QPointF startPinPoint;
        QSizeF startSize;
        WindowData(): 
            grabbedSection(Qt::NoSection), 
            closeButtonHovered(false), 
            closeButtonGrabbed(false)
        {}
    } *windowData;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};


#endif // QN_GRAPHICS_WIDGET_P_H
