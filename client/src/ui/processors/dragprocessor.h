#ifndef QN_DRAG_PROCESSOR_H
#define QN_DRAG_PROCESSOR_H

#include <QObject>
#include <QWeakPointer>
#include <ui/common/scene_utility.h>
#include "draginfo.h"
#include "dragprocesshandler.h"

class QWidget;
class QMouseEvent;
class QPaintEvent;

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

    void transitionInternalHelper(State newState);

    void transitionInternal(QEvent *event, State newState);

    void transition(QEvent *event, State state);

    void transition(QEvent *event, QGraphicsView *view, State state);

    void transition(QEvent *event, QGraphicsScene *scene, State state);

    void transition(QEvent *event, QGraphicsItem *item, State state);
    
    void drag(QEvent *event, const QPoint &screenPos, const QPointF &scenePos);

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
