#ifndef QN_DRAG_PROCESSOR_H
#define QN_DRAG_PROCESSOR_H

#include <QObject>
#include <QWeakPointer>
#include <utils/common/scene_utility.h>

class QWidget;
class QMouseEvent;
class QPaintEvent;
class QGraphicsView;

class DragProcessor;

class DragProcessHandler {
public:
    DragProcessHandler();
    virtual ~DragProcessHandler();

protected:
    virtual void startDragProcess(QGraphicsView *) {};
    virtual void startDrag(QGraphicsView *) {};
    virtual void drag(QGraphicsView *) {};
    virtual void finishDrag(QGraphicsView *) {};
    virtual void finishDragProcess(QGraphicsView *) {};

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
        INITIAL,
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

    void mousePressEvent(QWidget *viewport, QMouseEvent *event);
    void mouseMoveEvent(QWidget *viewport, QMouseEvent *event);
    void mouseReleaseEvent(QWidget *viewport, QMouseEvent *event);
    void paintEvent(QWidget *viewport, QPaintEvent *event);

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void startDragTimer();
    void killDragTimer();
    void transition(State state);
    void transition(QGraphicsView *view, State state);
    void drag(QGraphicsView *view, const QPoint &pos);
    void checkThread(QWidget *viewport);

private:
    /** Flags. */
    Flags m_flags;

    /** Drag handler. */
    DragProcessHandler *m_handler;

    /** Current drag state. */
    State m_state;

    /** Button that triggered the current drag operation. Releasing this button will finish current drag operation. */
    Qt::MouseButton m_triggerButton;

    /** Current graphics view. */
    QWeakPointer<QGraphicsView> m_view;

    /** Position in view coordinates where trigger button was pressed. */
    QPoint m_mousePressPos;

    /** Position in scene coordinates where trigger button was pressed. */
    QPointF m_mousePressScenePos;

    /** Mouse position in view coordinates where the last event was processed. */
    QPoint m_lastMousePos;

    /** Mouse position in scene coordinates where the last event was processed. */
    QPointF m_lastMouseScenePos;

    /** Current mouse position in view coordinates. */
    QPoint m_mousePos;

    /** Current mouse position in scene coordinates. */
    QPointF m_mouseScenePos;

    /** Identifier of a timer used to track mouse press time. */
    int m_dragTimerId;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DragProcessor::Flags);

#endif // QN_DRAG_PROCESSOR_H
