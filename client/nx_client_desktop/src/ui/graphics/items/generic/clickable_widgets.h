#ifndef QN_CLICKABLE_WIDGETS_H
#define QN_CLICKABLE_WIDGETS_H

#include <ui/processors/clickable.h>

#include <ui/graphics/items/standard/graphics_widget.h>

#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>

/**
 * Graphics widget that provides signals for mouse click and double click events.
 */
class QnClickableWidget: public Clickable<GraphicsWidget, QnClickableWidget> {
    Q_OBJECT
    typedef Clickable<GraphicsWidget, QnClickableWidget> base_type;

public:
    QnClickableWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0): base_type(parent, wFlags) {}

signals:
    void clicked(Qt::MouseButton button);
    void doubleClicked(Qt::MouseButton button);
};


/**
 * Simple frame widget that provides signals for mouse click and double click events.
 */
class QnClickableFrameWidget: public Clickable<QnFramedWidget, QnClickableFrameWidget> {
    Q_OBJECT
    typedef Clickable<QnFramedWidget, QnClickableFrameWidget> base_type;

public:
    QnClickableFrameWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0): base_type(parent, wFlags) {}

signals:
    void clicked(Qt::MouseButton button);
    void doubleClicked(Qt::MouseButton button);
};


/**
 * Proxy label widget that provides signals for mouse click and double click events.
 */
class QnClickableProxyLabel: public Clickable<QnProxyLabel, QnClickableProxyLabel> {
    Q_OBJECT
    typedef Clickable<QnProxyLabel, QnClickableProxyLabel> base_type;

public:
    QnClickableProxyLabel(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0): base_type(parent, wFlags) {}

signals:
    void clicked(Qt::MouseButton button);
    void doubleClicked(Qt::MouseButton button);
};

#endif // QN_CLICKABLE_WIDGETS_H
