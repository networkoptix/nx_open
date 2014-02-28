#ifndef QN_CLICKABLE_H
#define QN_CLICKABLE_H

#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <utils/math/fuzzy.h>
#include <utils/common/forward.h>

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
    QN_FORWARD_CONSTRUCTOR(Clickable, Base, {m_clickableButtons = 0; m_isDoubleClick = false;})

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

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        if(!(event->button() & m_clickableButtons))
            return;

        event->accept();
        m_isDoubleClick = event->type() == QEvent::GraphicsSceneMouseDoubleClick;
        
        pressedNotify(event);
    }

    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
        if(!(event->button() & m_clickableButtons))
            return;

        releasedNotify(event);

        if(!qFuzzyContains(this->boundingRect(), event->pos()))
            return; /* If the button was released outside the item boundaries, then it is not a click. */

        event->accept();

        if(m_isDoubleClick) {
            emit doubleClicked(event->button());
        } else {
            emit clicked(event->button());
        }

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

    /* Virtual signals follow. These can be overridden in derived class with actual signals. */

    virtual void clicked(Qt::MouseButton button) { Q_UNUSED(button); }
    virtual void doubleClicked(Qt::MouseButton button) { Q_UNUSED(button); }

private:
    Qt::MouseButtons m_clickableButtons;
    bool m_isDoubleClick;
};

#endif // QN_CLICKABLE_H
