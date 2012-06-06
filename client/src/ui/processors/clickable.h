#ifndef QN_CLICKABLE_H
#define QN_CLICKABLE_H

#include <QGraphicsSceneMouseEvent>
#include <ui/common/geometry.h>

/**
 * A mixin class that lets graphics items handle mouse clicks properly.
 * Not a processor because the logic it implements is pretty simple.
 * 
 * Registered clicks are to be processed by overriding the <tt>clickedNotify</tt> 
 * function in derived class.
 */
template<class Base>
class Clickable: public Base {
public:
    template<class T0>
    Clickable(const T0 &arg0): Base(arg0), m_clickableButtons(0) {}

    template<class T0, class T1>
    Clickable(const T0 &arg0, const T1 &arg1): Base(arg0, arg1), m_clickableButtons(0) {}

protected:
    Qt::MouseButtons clickableButtons() const {
        return m_clickableButtons;
    }

    /**
     * Sets the mouse buttons that are clickable. By default, no mouse buttons
     * are clickable.
     * 
     * \param clickableButtons          Clickable buttons.
     */
    void setClickableButtons(Qt::MouseButtons clickableButtons) {
        m_clickableButtons = clickableButtons;
    }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        if(!(event->button() & m_clickableButtons))
            return;

        event->accept();
        pressedNotify(event);
    }

    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
        if(!(event->button() & m_clickableButtons))
            return;

        releasedNotify(event);

        if(!qFuzzyContains(this->boundingRect(), event->pos()))
            return; /* If the button was released outside the item boundaries, then it is not a click. */

        event->accept();
        clickedNotify(event);
    }

    /**
     * This function is called each time a mouse click on this item is registered.
     * Handling it is up to derived class.
     * 
     * \param event                     Event that caused the mouse click to be generated.
     */
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event); };

    virtual void pressedNotify(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event); };

    virtual void releasedNotify(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event); };

private:
    Qt::MouseButtons m_clickableButtons;
};

#endif QN_CLICKABLE_H
