#ifndef QN_WORKBENCH_CONTROLLER_H
#define QN_WORKBENCH_CONTROLLER_H

#include <QObject>
#include <QScopedPointer>
#include <ui/common/geometry.h>
#include <ui/actions/actions.h>
#include <core/resource/resource_fwd.h>
#include "workbench_globals.h"
#include "workbench_context_aware.h"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QGraphicsWidget;
class QMenu;
class QLabel;
class QPropertyAnimation;

class CLVideoCamera;

class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class MoveInstrument;
class RubberBandInstrument;
class ResizingInstrument;
class DropInstrument;
class DragInstrument;
class RotationInstrument;
class MotionSelectionInstrument;
class ForwardingInstrument;
class ClickInstrument;
class ClickInfo;
class ResizingInfo;

class QnToggle;
class QnActionManager;
class QnWorkbenchDisplay;
class QnWorkbenchLayout;
class QnWorkbench;
class QnResourceWidget;
class QnWorkbenchItem;
class QnWorkbenchGridMapper;
class QnScreenRecorder;

/**
 * This class implements default scene manipulation logic.
 */
class QnWorkbenchController: public QObject, public QnWorkbenchContextAware, protected QnGeometry {
    Q_OBJECT;

public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent for this object.
     */
    QnWorkbenchController(QObject *parent = NULL);

    /**
     * Virtual destructor. 
     */
    virtual ~QnWorkbenchController();

    QnWorkbenchGridMapper *mapper() const;

    MotionSelectionInstrument *motionSelectionInstrument() const {
	    return m_motionSelectionInstrument;
    }

    ClickInstrument *itemRightClickInstrument() const {
        return m_itemRightClickInstrument;
    }

    MoveInstrument *moveInstrument() const {
        return m_moveInstrument;
    }

    ForwardingInstrument *itemMouseForwardingInstrument() const {
        return m_itemMouseForwardingInstrument;
    }

    ResizingInstrument *resizingInstrument() const {
        return m_resizingInstrument;
    }

    RubberBandInstrument *rubberBandInstrument() const {
        return m_rubberBandInstrument;
    }

    ClickInstrument *itemLeftClickInstrument() const {
        return m_itemLeftClickInstrument;
    }

public slots:
    void startRecording();
    void stopRecording();

protected:
    bool eventFilter(QObject *watched, QEvent *event);

    void updateGeometryDelta(QnResourceWidget *widget);
    void displayMotionGrid(const QList<QnResourceWidget *> &widgets, bool display);

    void moveCursor(const QPoint &direction);

protected slots:
    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void at_resizing(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);

    void at_moveStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_move(QGraphicsView *view, const QPointF &totalDelta);
    void at_moveFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);

    void at_rotationStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void at_rotationFinished(QGraphicsView *view, QGraphicsWidget *widget);

    void at_motionSelectionProcessStarted(QGraphicsView *view, QnResourceWidget *widget);
    void at_motionRegionCleared(QGraphicsView *view, QnResourceWidget *widget);
    void at_motionRegionSelected(QGraphicsView *view, QnResourceWidget *widget, const QRect &region);

    void at_item_leftClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_rightClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_middleClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_doubleClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);

    void at_scene_clicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_leftClicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_rightClicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_doubleClicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_keyPressed(QGraphicsScene *scene, QEvent *event);

    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_display_widgetChanged(Qn::ItemRole role);

    void at_widget_rotationStartRequested(QnResourceWidget *widget);
    void at_widget_rotationStartRequested();
    void at_widget_rotationStopRequested();

    void at_selectAllAction_triggered();
    void at_showMotionAction_triggered();
    void at_hideMotionAction_triggered();
    void at_toggleMotionAction_triggered();
    void at_maximizeItemAction_triggered();
    void at_unmaximizeItemAction_triggered();
    void at_recordingAction_triggered(bool checked);
    void at_fitInViewAction_triggered();
    void at_checkFileSignatureAction_triggered();

    void at_screenRecorder_error(const QString &errorMessage);
    void at_screenRecorder_recordingStarted();
    void at_screenRecorder_recordingFinished(const QString &recordedFileName);

    void at_recordingAnimation_valueChanged(const QVariant &value);
    void at_recordingAnimation_finished();

private:
    /* Global state. */

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[Qn::ItemRoleCount];

    /** Zoomed state toggle. */
    QnToggle *m_zoomedToggle;


    /* Instruments. */

    /** Hand scroll instrument. */
    HandScrollInstrument *m_handScrollInstrument;

    /** Wheel zoom instrument. */
    WheelZoomInstrument *m_wheelZoomInstrument;

    /** Widget moving instrument. */
    MoveInstrument *m_moveInstrument;

    /** Rotation instrument. */
    RotationInstrument *m_rotationInstrument;

    /** Rubber band instrument. */
    RubberBandInstrument *m_rubberBandInstrument;

    /** Resizing instrument. */
    ResizingInstrument *m_resizingInstrument;

    /** Drop instrument. */
    DropInstrument *m_dropInstrument;

    /** Drag instrument. */
    DragInstrument *m_dragInstrument;

    /** Motion selection instrument. */
    MotionSelectionInstrument *m_motionSelectionInstrument;

    /** Item right click instrument. */
    ClickInstrument *m_itemRightClickInstrument;

    /** Item mouse forwarding instrument. */
    ForwardingInstrument *m_itemMouseForwardingInstrument;

    /** Instrument that tracks left clicks on items. */
    ClickInstrument *m_itemLeftClickInstrument;



    /* Keyboard control-related state. */

    /** Last keyboard cursor position. */
    QPoint m_cursorPos;

    /** Last item that was affected by keyboard control. */
    QWeakPointer<QnWorkbenchItem> m_cursorItem;


    /* Resizing-related state. */

    /** Widget that is being resized. */
    QnResourceWidget *m_resizedWidget;

    /** Current grid rect of the widget being resized. */
    QRect m_resizedWidgetRect;



    /* Dragging-related state. */

    /** Workbench items that are being dragged. */
    QList<QnWorkbenchItem *> m_draggedWorkbenchItems;

    /** Workbench items that will be replaced as a result of a drag. */
    QList<QnWorkbenchItem *> m_replacedWorkbenchItems;

    /** Current drag distance. */
    QPoint m_dragDelta;

    /** Target geometries for concatenation of dragged and replaced item lists. */
    QList<QRect> m_dragGeometries;



    /* Screen recording-related state. */

    /** Screen recorder object. */
    QnScreenRecorder *m_screenRecorder;

    /** Whether recorder countdown is canceled. */
    bool m_countdownCanceled;

    /** Screen recording countdown label. */
    QLabel *m_recordingLabel;

    /** Animation for screen recording countdown. */
    QPropertyAnimation *m_recordingAnimation;

};

#endif // QN_WORKBENCH_CONTROLLER_H
