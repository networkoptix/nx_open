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
template<class Base, class Derived = void>
class Clickable: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Clickable, Base, {m_clickableButtons = 0; m_isDoubleClick = false; m_skipDoubleClick = false;})

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

    /** Set the flag to skip next double-click. Used to workaround invalid double click when
     *  the first mouse click was handled and changed the widget state. */
    void skipDoubleClick() {
        m_skipDoubleClick = true;
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

        if(m_isDoubleClick && !m_skipDoubleClick) {
            emitDoubleClicked(static_cast<Derived *>(this), event);
        } else {
            emitClicked(static_cast<Derived *>(this), event);
        }
        m_skipDoubleClick = false;

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
    /* We cannot implement these as virtual signals because virtual signals are
     * not supported by new connect syntax. So we use CRTP. Nice bonus is that 
     * there is no need to befriend base class as signals are public in Qt5. */

    void clicked(Qt::MouseButton) {}
    void doubleClicked(Qt::MouseButton) {}

    template<class T>
    void emitClicked(T *target, QGraphicsSceneMouseEvent *event) { target->clicked(event->button()); }
    void emitClicked(void *, QGraphicsSceneMouseEvent *) {}

    template<class T>
    void emitDoubleClicked(T *target, QGraphicsSceneMouseEvent *event) { target->doubleClicked(event->button()); }
    void emitDoubleClicked(void *, QGraphicsSceneMouseEvent *) {}

private:
    Qt::MouseButtons m_clickableButtons;
    bool m_isDoubleClick;
    bool m_skipDoubleClick;
};

#endif // QN_CLICKABLE_H
