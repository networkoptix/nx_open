#ifndef QN_WORKBENCH_CONTROLLER_H
#define QN_WORKBENCH_CONTROLLER_H

#include <QObject>
#include <QScopedPointer>
#include <ui/common/scene_utility.h>
#include <core/resource/resource_fwd.h>
#include "workbench.h"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QMenu;
class QLabel;
class QPropertyAnimation;

class CLVideoCamera;

class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class DragInstrument;
class RubberBandInstrument;
class ResizingInstrument;
class DropInstrument;
class RotationInstrument;
class MotionSelectionInstrument;
class ForwardingInstrument;
class ClickInstrument;
class ClickInfo;
class ResizingInfo;

class QnWorkbenchDisplay;
class QnWorkbenchLayout;
class QnWorkbench;
class QnResourceWidget;
class QnWorkbenchItem;
class QnWorkbenchGridMapper;
class QnScreenRecorder;

/**
 * This class implements default scene manipulation logic.
 *
 * It also presents some functions for high-level scene content manipulation.
 */
class QnWorkbenchController: public QObject, protected SceneUtility {
    Q_OBJECT;

public:
    /**
     * Constructor.
     * 
     * \param display                   Display to be used by this controller. Must not be NULL.
     * \param parent                    Parent for this object.
     */
    QnWorkbenchController(QnWorkbenchDisplay *display, QObject *parent = NULL);

    /**
     * Virtual destructor. 
     */
    virtual ~QnWorkbenchController();

    QnWorkbenchDisplay *display() const;

    QnWorkbench *workbench() const;

    QnWorkbenchGridMapper *mapper() const;

    void drop(const QUrl &url, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QList<QUrl> &urls, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QString &file, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QList<QString> &files, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QnResourcePtr &resource, const QPointF &gridPos = QPointF());
    void drop(const QnResourceList &resources, const QPointF &gridPos = QPointF());

    MotionSelectionInstrument *motionSelectionInstrument() const {
	    return m_motionSelectionInstrument;
    }

    ClickInstrument *itemRightClickInstrument() const {
        return m_itemRightClickInstrument;
    }

    DragInstrument *dragInstrument() const {
        return m_dragInstrument;
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

public slots:
    void startRecording();
    void stopRecording();

protected:
    bool eventFilter(QObject *watched, QEvent *event);

    void updateGeometryDelta(QnResourceWidget *widget);
    void displayMotionGrid(const QList<QGraphicsItem *> &items, bool display);

protected slots:
    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void at_resizing(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);

    void at_dragStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_drag(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_dragFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);

    void at_rotationStarted(QGraphicsView *view, QnResourceWidget *widget);
    void at_rotationFinished(QGraphicsView *view, QnResourceWidget *widget);

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

    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_display_widgetChanged(QnWorkbench::ItemRole role);

    void at_showMotionAction_triggered();
    void at_hideMotionAction_triggered();
    void at_cameraSettingsAction_triggered();

    void at_recordingAction_triggered(bool checked);
    void at_recordingSettingsAction_triggered();

    void at_screenRecorder_error(const QString &errorMessage);
    void at_screenRecorder_recordingStarted();
    void at_screenRecorder_recordingFinished(const QString &recordedFileName);

    void at_recordingAnimation_valueChanged(const QVariant &value);
    void at_recordingAnimation_finished();

private:
    /* Global state. */

    /** Workbench display. */
    QnWorkbenchDisplay *m_display;

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[QnWorkbench::ITEM_ROLE_COUNT];


    /* Instruments. */

    /** Hand scroll instrument. */
    HandScrollInstrument *m_handScrollInstrument;

    /** Wheel zoom instrument. */
    WheelZoomInstrument *m_wheelZoomInstrument;

    /** Dragging instrument. */
    DragInstrument *m_dragInstrument;

    /** Rotation instrument. */
    RotationInstrument *m_rotationInstrument;

    /** Rubber band instrument. */
    RubberBandInstrument *m_rubberBandInstrument;

    /** Resizing instrument. */
    ResizingInstrument *m_resizingInstrument;

    /** Archive drop instrument. */
    DropInstrument *m_archiveDropInstrument;

    /** Motion selection instrument. */
    MotionSelectionInstrument *m_motionSelectionInstrument;

    /** Item right click instrument. */
    ClickInstrument *m_itemRightClickInstrument;

    /** Item mouse forwarding instrument. */
    ForwardingInstrument *m_itemMouseForwardingInstrument;


    /* Resizing-related state. */

    /** Widget that is being resized. */
    QnResourceWidget *m_resizedWidget;

    /** Current grid rect of the widget being resized. */
    QRect m_resizedWidgetRect;


    /* Dragging-related state. */

    /** Items that are being dragged. */
    QList<QGraphicsItem *> m_draggedItems;

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
