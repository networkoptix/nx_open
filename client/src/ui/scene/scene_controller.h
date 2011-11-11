#ifndef QN_SCENE_CONTROLLER_H
#define QN_SCENE_CONTROLLER_H

#include <QObject>
#include <QScopedPointer>
#include <utils/common/scene_utility.h>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;

class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class DragInstrument;
class RubberBandInstrument;

class QnDisplaySynchronizer;
class QnDisplayState;
class QnDisplayWidget;

class QnSceneController: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    QnSceneController(QnDisplaySynchronizer *synchronizer, QObject *parent = NULL);

    virtual ~QnSceneController();

    
protected:
    void updateGeometryDelta(QnDisplayWidget *widget);

protected slots:
    void at_widget_resizingStarted();
    void at_widget_resizingFinished();

    void at_draggingStarted(QGraphicsView *view, QList<QGraphicsItem *> items);
    void at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items);

    void at_item_clicked(QGraphicsView *view, QGraphicsItem *item);
    void at_item_doubleClicked(QGraphicsView *view, QGraphicsItem *item);

    void at_scene_clicked(QGraphicsView *view);
    void at_scene_doubleClicked(QGraphicsView *view);

    // TODO: remove
    void at_widgetAdded(QnDisplayWidget *widget);
   

private:
    /** Display synchronizer. */
    QnDisplaySynchronizer *m_synchronizer;

    /** Display state. */
    QnDisplayState *m_state;

    /** Instrument manager for the scene. */ 
    InstrumentManager *m_manager;

    /** Graphics scene. */
    QGraphicsScene *m_scene;

    /** Graphics view. */
    QGraphicsView *m_view;

    /** Hand scroll instrument. */
    HandScrollInstrument *m_handScrollInstrument;

    /** Wheel zoom instrument. */
    WheelZoomInstrument *m_wheelZoomInstrument;

    /** Dragging instrument. */
    DragInstrument *m_dragInstrument;

    /** Rubber band instrument. */
    RubberBandInstrument *m_rubberBandInstrument;
};

#endif // QN_SCENE_CONTROLLER_H
