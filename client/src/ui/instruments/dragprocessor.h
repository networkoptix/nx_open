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

class DragProcessHandler {
public:
    DragProcessHandler();
    virtual ~DragProcessHandler();

protected:
    virtual void startDragProcess() {};
    virtual void startDrag() {};
    virtual void drag() {};
    virtual void finishDrag() {};
    virtual void finishDragProcess() {};

    DragProcessor *processor() const {
        return m_processor;
    }

private:
    friend class DragProcessor;

    DragProcessor *m_processor;
};


class DragProcessor: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    enum State {
        WAITING,
        PREPAIRING,
        DRAGGING
    };

    enum Flag {
        /** By default, drag processor will compress consecutive drag operations 
         * if scene and view mouse coordinates didn't change between them. 
         * This flag disables this compression. */
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

    void reset();

    const QPoint &mousePressScreenPos() const {
        return m_mousePressScreenPos;
    }

    const QPoint &lastMouseScreenPos() const {
        return m_lastMouseScreenPos;
    }

    const QPoint &mouseScreenPos() const {
        return m_mouseScreenPos;
    }

    QPoint mouseViewportPos() const;

    const QPointF &mousePressScenePos() const {
        return m_mousePressScenePos;
    }

    const QPointF &lastMouseScenePos() const {
        return m_lastMouseScenePos;
    }

    const QPointF &mouseScenePos() const {
        return m_mouseScenePos;
    }

    Qt::KeyboardModifiers modifiers() const {
        return m_modifiers;
    }

    QGraphicsView *view() const {
        return m_view;
    }

    void mousePressEvent(QWidget *viewport, QMouseEvent *event);

    void mouseMoveEvent(QWidget *viewport, QMouseEvent *event);

    void mouseReleaseEvent(QWidget *viewport, QMouseEvent *event);

    void paintEvent(QWidget *viewport, QPaintEvent *event);

    QGraphicsScene *scene() const {
        return m_scene;
    }

    void mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    void mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    void mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    QGraphicsItem *item() const {
        return m_item;
    }

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

    /** Counter that is used to detect nested transitions. */
    int m_transitionCounter;

    /** Current keyboard modifiers. */
    Qt::KeyboardModifiers m_modifiers;

    /** Button that triggered the current drag operation. Releasing this button will finish current drag operation. */
    Qt::MouseButton m_triggerButton;

    /** Current viewport. */
    QWidget *m_viewport;

    /** Current object that is being worked on. */
    QWeakPointer<QObject> m_object;

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

    /** Identifier of a timer used to track mouse press time. */
    int m_dragTimerId;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DragProcessor::Flags);

#endif // QN_DRAG_PROCESSOR_H
