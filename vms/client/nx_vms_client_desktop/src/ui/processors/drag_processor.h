#ifndef QN_DRAG_PROCESSOR_H
#define QN_DRAG_PROCESSOR_H

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include "drag_info.h"
#include "drag_process_handler.h"

class QWidget;
class QMouseEvent;
class QPaintEvent;
class QGraphicsSceneMouseEvent;

class DragProcessHandler;

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
class DragProcessor: public QObject
{
    Q_OBJECT
    Q_FLAGS(Flags Flag)
    Q_ENUMS(State)

public:
    enum State {
        Waiting,    /**< Waiting for the drag process to start. */
        Prepairing, /**< Drag process started, waiting for actual drag to start. */
        Running     /**< Drag is in progress. */
    };

    enum Flag {
        /** By default, drag processor will compress consecutive drag operations 
         * if scene and view mouse coordinates didn't change between them. 
         * This flag disables the compression. */
        DontCompress = 0x1, // TODO: #Elric #enum
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    DragProcessor(QObject* parent = nullptr);

    virtual ~DragProcessor() override;

    State state() const {
        return m_state;
    }

    bool isWaiting() const {
        return m_state == Waiting;
    }

    bool isPrepairing() const {
        return m_state == Prepairing;
    }

    bool isRunning() const {
        return m_state == Running;
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

    int startDragDistance() const {
        return m_startDragDistance;
    }

    void setStartDragDistance(int startDragDistance);

    int startDragTime() const {
        return m_startDragTime;
    }

    void setStartDragTime(int startDragTime);

    /**
     * Resets this drag processor, moving it into <tt>Waiting</tt> state. 
     */
    void reset();

    /* Set of widget-level event handler functions that are to be used from user code
     * in case drag processor is used outside the graphics view framework. */

    void widgetEvent(QWidget *widget, QEvent *event);

    void widgetMousePressEvent(QWidget *widget, QMouseEvent *event, bool instantDrag = false);

    void widgetMouseMoveEvent(QWidget *widget, QMouseEvent *event);

    void widgetMouseReleaseEvent(QWidget *widget, QMouseEvent *event);

    void widgetPaintEvent(QWidget *widget, QPaintEvent *event);


    /* Set of view-level event handler functions that are to be used from user code. */

    void mousePressEvent(QWidget *viewport, QMouseEvent *event, bool instantDrag = false);

    void mouseMoveEvent(QWidget *viewport, QMouseEvent *event);

    void mouseReleaseEvent(QWidget *viewport, QMouseEvent *event);

    void paintEvent(QWidget *viewport, QPaintEvent *event);


    /* Set of scene-level event handler functions that are to be used from user code. */

    void mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event, bool instantDrag = false);

    void mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    void mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    
    /* Set of item-level event handler functions that are to be used from user code. */

    void mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event, bool instantDrag = false);

    void mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);

    void mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);


    void redrag();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void startDragTimer();
    void killDragTimer();

    void checkThread(QObject *object);
    void checkThread(QGraphicsItem *item);

    void transitionInternalHelper(State newState);

    void transitionInternal(QEvent *event, State newState);

    void transition(QEvent *event, State state);

    void transition(QEvent *event, QWidget *widget, State state);

    void transition(QEvent *event, QGraphicsView *view, State state);

    void transition(QEvent *event, QGraphicsScene *scene, State state);

    void transition(QEvent *event, QGraphicsItem *item, State state);
    
    void drag(QEvent *event, const QPoint &screenPos, const QPointF &scenePos, const QPointF &itemPos, bool alwaysHandle);

    QPoint screenPos(QWidget *widget, QMouseEvent *event);

    template<class T>
    QPoint screenPos(T *object, QGraphicsSceneMouseEvent *event);

    QPointF scenePos(QWidget *widget, QMouseEvent *event);

    QPointF scenePos(QGraphicsView *view, QMouseEvent *event);

    template<class T>
    QPointF scenePos(T *object, QGraphicsSceneMouseEvent *event);

    template<class T, class Event>
    QPointF itemPos(T *object, Event *event);

    QPointF itemPos(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);

    template<class T, class Event>
    void mousePressEventInternal(T *object, Event *event, bool instantDrag);

    template<class T, class Event>
    void mouseMoveEventInternal(T *object, Event *event);

    template<class T, class Event>
    void mouseReleaseEventInternal(T *object, Event *event);

    static QGraphicsView *view(QWidget *viewport);

private:
    /** Flags. */
    Flags m_flags;

    /** Whether handler is currently running. */
    bool m_handling;

    /** Whether handler should be restarted. */
    bool m_rehandle;

    /** Start drag distance. */
    int m_startDragDistance;

    /** Start drag timeout, in milliseconds. */
    int m_startDragTime;

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
    QPointer<QObject> m_object;

    /** Identifier of a timer used to track mouse press time. */
    int m_dragTimerId; // TODO: #Elric use QBasicTimer

    /** Whether the first drag notification was sent. */
    bool m_firstDragSent;

    /** Drag information. */
    DragInfo m_info;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DragProcessor::Flags);

#endif // QN_DRAG_PROCESSOR_H
