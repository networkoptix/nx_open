#ifndef QN_CLICKABLE_WIDGETS_H
#define QN_CLICKABLE_WIDGETS_H

#include <ui/processors/clickable.h>

#include <ui/graphics/items/standard/graphics_widget.h>

#include "simple_frame_widget.h"

/**
 * Graphics widget that provides signals for mouse click and double click events.
 */
class QnClickableWidget: public Clickable<GraphicsWidget> {
    Q_OBJECT
    typedef Clickable<GraphicsWidget> base_type;

public:
    QnClickableWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0): base_type(parent, wFlags) {}

signals:
    void clicked();
    void clicked(Qt::MouseButton button);
    void doubleClicked();
    void doubleClicked(Qt::MouseButton button);
};


/**
 * Simple frame widget that provides signals for mouse click and double click events.
 */
class QnClickableFrameWidget: public Clickable<QnSimpleFrameWidget> {
    Q_OBJECT;
    typedef Clickable<QnSimpleFrameWidget> base_type;

public:
    QnClickableFrameWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0): base_type(parent, wFlags) {}

signals:
    void clicked();
    void clicked(Qt::MouseButton button);
    void doubleClicked();
    void doubleClicked(Qt::MouseButton button);
};


#endif // QN_CLICKABLE_WIDGETS_H
