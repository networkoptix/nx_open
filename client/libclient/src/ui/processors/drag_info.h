#ifndef QN_DRAG_INFO_H
#define QN_DRAG_INFO_H

#include <QtCore/QPoint>

class QGraphicsView;
class QGraphicsItem;
class QGraphicsScene;

// TODO: #Elric refactor Drag* classes into templates.
// * DragProcessor<QWidget>
// * DragProcessor<QGraphicsView>
// * DragProcessor<QGraphicsScene>
// * DragProcessor<QGraphicsItem>

/**
 * Dragging information that is passed to handler on each drag move.
 */
class DragInfo {
public:
    DragInfo(): m_event(NULL) {}

    /**
     * \returns                         Position of the mouse pointer at the moment the 
     *                                  mouse button that initiated this drag process was pressed, 
     *                                  in screen coordinates.
     */
    const QPoint &mousePressScreenPos() const {
        return m_mousePressScreenPos;
    }

    /**
     * \returns                         Position of the mouse pointer at the last call to <tt>drag()</tt> 
     *                                  handler function, in screen coordinates.
     *                                  When <tt>drag()</tt> is called for the first time, 
     *                                  this function returns mouse press position.
     */
    const QPoint &lastMouseScreenPos() const {
        return m_lastMouseScreenPos;
    }

    /**
     * \returns                         Current position of the mouse pointer, in screen coordinates.
     */
    const QPoint &mouseScreenPos() const {
        return m_mouseScreenPos;
    }

    /**
     * \returns                         Current position of the mouse pointer, in viewport coordinates. 
     */
    QPoint mouseViewportPos() const;

    /**
     * \returns                         Position of the mouse pointer at the moment the 
     *                                  mouse button that initiated this drag process was pressed, 
     *                                  in scene coordinates.
     */
    const QPointF &mousePressScenePos() const {
        return m_mousePressScenePos;
    }

    /**
     * \returns                         Position of the mouse pointer at the last call to <tt>drag()</tt> 
     *                                  handler function, in scene coordinates.
     *                                  When <tt>drag()</tt> is called for the first time, 
     *                                  this function returns mouse press position.
     */
    const QPointF &lastMouseScenePos() const {
        return m_lastMouseScenePos;
    }

    /**
     * \returns                         Current position of the mouse pointer, in scene coordinates. 
     */
    const QPointF &mouseScenePos() const {
        return m_mouseScenePos;
    }

    /**
     * \returns                         Position of the mouse pointer at the moment the 
     *                                  mouse button that initiated this drag process was pressed, 
     *                                  in item coordinates.
     */
    const QPointF &mousePressItemPos() const {
        return m_mousePressItemPos;
    }

    /**
     * \returns                         Position of the mouse pointer at the last call to <tt>drag()</tt> 
     *                                  handler function, in item coordinates.
     *                                  When <tt>drag()</tt> is called for the first time, 
     *                                  this function returns mouse press position.
     */
    const QPointF &lastMouseItemPos() const {
        return m_lastMouseItemPos;
    }

    /**
     * \returns                         Current position of the mouse pointer, in item coordinates. 
     */
    const QPointF &mouseItemPos() const {
        return m_mouseItemPos;
    }


    /**
     * \returns                         Current keyboard modifiers. 
     */
    Qt::KeyboardModifiers modifiers() const;

    /**
     * Note that this function returns NULL if current widget is 
     * being destroyed or if non-widget set of event handler functions is used.
     *
     * \returns                         Current graphics view. 
     */
    QWidget *widget() const {
        return m_widget;
    }

    /**
     * Note that this function returns NULL if current graphics view is 
     * being destroyed or if non-view set of event handler functions is used.
     *
     * \returns                         Current graphics view. 
     */
    QGraphicsView *view() const {
        return m_view;
    }

    /**
     * Note that this function returns NULL if current graphics scene is 
     * being destroyed or if non-scene set of event handler functions is used.
     * 
     * \returns                         Current graphics scene.
     */
    QGraphicsScene *scene() const {
        return m_scene;
    }

    /**
     * Note that this function returns NULL if current graphics item is 
     * being destroyed or if non-item set of event handler functions is used.
     * 
     * \returns                         Current graphics item.
     */
    QGraphicsItem *item() const {
        return m_item;
    }

private:
    friend class DragProcessor;

    /** Current event. */
    QEvent *m_event;

    /** Button that triggered the current drag operation. Releasing this button will finish current drag operation. */
    Qt::MouseButton m_triggerButton;

    /** Current widget. */
    QWidget *m_widget;

    /** Current graphics view. */
    QGraphicsView *m_view;

    /** Current graphics scene. */
    QGraphicsScene *m_scene;

    /** Current graphics item. */
    QGraphicsItem *m_item;

    /** Position in view coordinates where trigger button was pressed. */
    QPoint m_mousePressScreenPos;

    /** Position in scene coordinates where trigger button was pressed. */
    QPointF m_mousePressScenePos;

    /** Position in item coordinates where trigger button was pressed. */
    QPointF m_mousePressItemPos;

    /** Mouse position in view coordinates where the last event was processed. */
    QPoint m_lastMouseScreenPos;

    /** Mouse position in scene coordinates where the last event was processed. */
    QPointF m_lastMouseScenePos;

    /** Mouse position in item coordinates where the last event was processed. */
    QPointF m_lastMouseItemPos;

    /** Current mouse position in view coordinates. */
    QPoint m_mouseScreenPos;

    /** Current mouse position in scene coordinates. */
    QPointF m_mouseScenePos;

    /** Current mouse position in item coordinates. */
    QPointF m_mouseItemPos;
};

#endif // QN_DRAG_INFO_H
