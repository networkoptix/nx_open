#ifndef QN_CLICKABLE_WIDGET_H
#define QN_CLICKABLE_WIDGET_H

#include <ui/processors/clickable.h>
#include <ui/graphics/items/standard/graphics_widget.h>

/**
 * Graphics widget that provides signals for mouse click and double click events.
 * 
 * Note that the signal is emitted when the mouse button is released.
 */
class QnClickableWidget: public Clickable<GraphicsWidget> {
    Q_OBJECT;

    typedef Clickable<GraphicsWidget> base_type;

public:
    QnClickableWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0): base_type(parent, wFlags) {}

    virtual ~QnClickableWidget() {};

    using base_type::clickableButtons;
    using base_type::setClickableButtons;

signals:
    void clicked();
    void doubleClicked();

protected:
    virtual void pressedNotify(QGraphicsSceneMouseEvent *event) {
        m_isDoubleClick = event->type() == QEvent::GraphicsSceneMouseDoubleClick;
    };

    virtual void clickedNotify(QGraphicsSceneMouseEvent *) override {
        if(m_isDoubleClick) {
            emit doubleClicked();
        } else {
            emit clicked();
        }
    };

private:
    bool m_isDoubleClick;
};


#endif // QN_CLICKABLE_WIDGET_H
