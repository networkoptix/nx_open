#ifndef QN_DRAG_PROCESSOR_H
#define QN_DRAG_PROCESSOR_H

#include <QObject>
#include <QWeakPointer>
#include <QPoint>
#include <utils/common/scene_utility.h>

class QWidget;
class QMouseEvent;
class QPaintEvent;
class QGraphicsView;
class QGraphicsItem;
class QGraphicsScene;

class DragProcessor;
class DragInfo;

/**
 * Interface for handling drag process state changes.
 */
class DragProcessHandler {
public:
    DragProcessHandler();
    virtual ~DragProcessHandler();

protected:
    /**
     * This function is called whenever drag process starts. It usually happens
     * when the user presses a mouse button.
     * 
     * It is guaranteed that <tt>finishDragProcess()</tt> will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void startDragProcess(DragInfo *info) { Q_UNUSED(info); };

    /**
     * This function is called whenever drag starts. It usually happens when
     * user moves the mouse far enough from the point where it was pressed,
     * or if he holds it pressed long enough.
     * 
     * Note that this function may not be called if the user releases the
     * mouse button before the drag could start.
     * 
     * It is guaranteed that if this function was called, then 
     * <tt>finishDrag()</tt> will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void startDrag(DragInfo *info) { Q_UNUSED(info); };

    /**
     * This function is called each time a mouse position changes while drag is
     * in progress.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void dragMove(DragInfo *info) { Q_UNUSED(info); };

    /**
     * This function is called whenever drag ends. It usually happens when the
     * user releases the mouse button that started the drag.
     * 
     * If <tt>startDrag()</tt> was called, then it is guaranteed that this 
     * function will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void finishDrag(DragInfo *info) { Q_UNUSED(info); };

    /**
     * This function is called whenever drag process ends. It usually happens 
     * right after the actual drag ends.
     * 
     * If <tt>startDragProcess()</tt> was called, then it is guaranteed that
     * this function will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void finishDragProcess(DragInfo *info) { Q_UNUSED(info); };

    /**
     * \returns                         Drag processor associated with this handler. 
     */
    DragProcessor *dragProcessor() const {
        return m_processor;
    }

private:
    friend class DragProcessor;

    DragProcessor *m_processor;
};


/**
 * Dragging information that is passed to handler on each drag move.
 */
class DragInfo {
public:
    DragInfo(): m_modifiers(0) {}

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
     * \returns                         Current keyboard modifiers. 
     */
    Qt::KeyboardModifiers modifiers() const {
        return m_modifiers;
    }

    /**
     * Note that this function returns NULL if current graphics view is 
     * being destroyed or if non-view set of event handler function is used.
     *
     * \returns                         Current graphics view. 
     */
    QGraphicsView *view() const {
        return m_view;
    }

    /**
     * Note that this function returns NULL if current graphics scene is 
     * being destroyed or if non-scene set of event handler function is used.
     * 
     * \returns                         Current graphics scene.
     */
    QGraphicsScene *scene() const {
        return m_scene;
    }

    /**
     * Note that this function returns NULL if current graphics item is 
     * being destroyed or if non-item set of event handler function is used.
     * 
     * \returns                         Current graphics item.
     */
    QGraphicsItem *item() const {
        return m_item;
    }

private:
    friend class DragProcessor;

    /** Button that triggered the current drag operation. Releasing this button will finish current drag operation. */
    Qt::MouseButton m_triggerButton;

    /** Current keyboard modifiers. */
    Qt::KeyboardModifiers m_modifiers;

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

    /** Mouse position in view coordinates where the last event was processed. */
    QPoint m_lastMouseScreenPos;

    /** Mouse position in scene coordinates where the last event was processed. */
    QPointF m_lastMouseScenePos;

    /** Current mouse position in view coordinates. */
    QPoint m_mouseScreenPos;

    /** Current mouse position in scene coordinates. */
    QPointF m_mouseScenePos;
};


/**
 * Drag processor implements logic that is common to drag-based instruments such
 * as hand scrolling instrument, dragging instrument, and even click and double
 * click detection instruments.
 * 
 * Dragging is not a simple process and drag distance and drag time must be 
 * taken into account. Drag processor takes care of all these problems
 * abstracting away the gory details.
 * 
 * Drag processor presents several event handling function that are to be
 * called from user code. Note that only one set of functions can be used for
 * a single drag processor (e.g. a single drag processor cannot be used to
 * track scene- and view-level dragging at the same time). This is not checked at
 * runtime, so be careful.
 * 
 * User code is notified about state changes through the <tt>DragProcessHandler</tt>
 * interface. It is guaranteed that no matter what happens, functions of this
 * interface will be called in proper order. For example, if <tt>startDrag()</tt>
 * function was called, then <tt>finishDrag()</tt> will also be called even
 * if this drag processor or target surface is destroyed.
 */
class DragProcessor: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    enum State {
        WAITING,    /**< Waiting for the drag process to start. */
        PREPAIRING, /**< Drag process started, waiting for actual drag to start. */
        DRAGGING    /**< Drag is in progress. */
    };

    enum Flag {
        /** By default, drag processor will compress consecutive drag operations 
         * if scene and view mouse coordinates didn't change between them. 
         * This flag disables the compression. */
        DONT_COMPRESS = 0x1 
    };
    Q_DECLARE_FLAGS(Flags, Flag);

    DragProcessor(QObject *parent = NULL);

    virtual ~DragProcessor();

    State state() const {
        return m_state;
    }

    bool isWaiting() const {
        return m_state == WAITING;
    }

    bool isPrepairing() const {
        return m_state == PREPAIRING;
    }

    bool isDragging() const {
        return m_state == DRAGGING;
    }

    Flags flags() const {
        return m_flags;
    }

    void setFlags(Flags flags) {
        m_flags = flags;
    }

    DragProcessHandler *handler() const {
        return m_handler;
    }

    void setHandler(DragProcessHandler *handler);

    /**
     * Resets this drag processor, moving it into <tt>WAITING</tt> state. 
     */
    void reset();


    /* Set of view-level event handler functions that are to be used from user code. */

    void mousePressEvent(QWidget *viewport, QMouseEvent *event);

    void mouseMoveEvent(QWidget *viewport, QMouseEvent *event);

    void mouseReleaseEvent(QWidget *viewport, QMouseEvent *event);

    void paintEvent(QWidget *viewport, QPaintEvent *event);


    /* Set of scene-level event handler functions that are to be used from user code. */

    void mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    void mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    void mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    
    /* Set of item-level event handler functions that are to be used from user code. */

    void mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);

    void mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);

    void mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void startDragTimer();
    void killDragTimer();

    void checkThread(QObject *object);
    void checkThread(QGraphicsItem *item);

    void preprocessEvent(QGraphicsSceneMouseEvent *event);
    void preprocessEvent(QMouseEvent *event);

    template<class Event>
    void transitionInternal(Event *event, State newState);

    template<class Event>
    void transition(Event *event, State state);

    template<class Event>
    void transition(Event *event, QGraphicsView *view, State state);

    template<class Event>
    void transition(Event *event, QGraphicsScene *scene, State state);

    template<class Event>
    void transition(Event *event, QGraphicsItem *item, State state);
    
    void drag(const QPoint &screenPos, const QPointF &scenePos);

    QPoint screenPos(QGraphicsView *view, QMouseEvent *event);
    template<class T>
    QPoint screenPos(T *object, QGraphicsSceneMouseEvent *event);

    QPointF scenePos(QGraphicsView *view, QMouseEvent *event);
    template<class T>
    QPointF scenePos(T *object, QGraphicsSceneMouseEvent *event);

    template<class T, class Event>
    void mousePressEventInternal(T *object, Event *event);

    template<class T, class Event>
    void mouseMoveEventInternal(T *object, Event *event);

    template<class T, class Event>
    void mouseReleaseEventInternal(T *object, Event *event);

    using QnSceneUtility::view;

private:
    /** Flags. */
    Flags m_flags;

    /** Drag handler. */
    DragProcessHandler *m_handler;

    /** Current drag state. */
    State m_state;

    /** Counter that is used to detect nested transitions.
     *
     * If a complex (two states long) transition is in progress, and handler
     * for the transition to the first state triggered another transition, then 
     * transition to the second state must not be performed. */
    int m_transitionCounter;

    /** Current viewport. Used to filter off paint events from other viewports. */
    QWidget *m_viewport;

    /** Current object that is being worked on. */
    QWeakPointer<QObject> m_object;

    /** Identifier of a timer used to track mouse press time. */
    int m_dragTimerId;

    /** Drag information. */
    DragInfo m_info;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DragProcessor::Flags);

#endif // QN_DRAG_PROCESSOR_H
