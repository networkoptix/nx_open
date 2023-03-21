// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtGui/QVector3D>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/utils/volatile_unique_ptr.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/graphics/instruments/click_info.h>
#include <utils/color_space/image_correction.h>

#include "workbench_context_aware.h"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QGraphicsWidget;
class QMenu;
class QLabel;
class QPropertyAnimation;

class Instrument;
class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class MoveInstrument;
class RubberBandInstrument;
class ResizingInstrument;
class DropInstrument;
class DragInstrument;
class PtzInstrument;
class RotationInstrument;
class MotionSelectionInstrument;
class ForwardingInstrument;
class ClickInstrument;
class ResizingInfo;
class ZoomWindowInstrument;
class ObjectTrackingInstrument;
class GridAdjustmentInstrument;

class QnToggle;
class QnWorkbenchDisplay;
class QnWorkbenchLayout;
class QnResourceWidget;
class QnMediaResourceWidget;
class QnWorkbenchItem;
class QnWorkbenchGridMapper;

class WeakGraphicsItemPointerList;

/**
 * This class implements default scene manipulation logic.
 */
class QnWorkbenchController: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;

public:
    /**
     * Constructor.
     *
     * \param parent                    Parent for this object.
     */
    QnWorkbenchController(QObject *parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchController() = default;

    QnWorkbenchGridMapper *mapper() const;

    Instrument* motionSelectionInstrument() const;

    bool isMenuEnabled() const {
        return m_menuEnabled;
    }

    void setMenuEnabled(bool menuEnabled) {
        m_menuEnabled = menuEnabled;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event);

    void updateGeometryDelta(QnResourceWidget *widget);
    void displayMotionGrid(const QList<QnResourceWidget *> &widgets, bool display);

    void moveCursor(const QPoint &aAxis, const QPoint &bAxis);
    void showContextMenuAt(const QPoint &pos);

    void updateDraggedItems();
protected slots:
    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void at_resizing(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);

    void at_moveStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_move(QGraphicsView *view, const QPointF &totalDelta);
    void at_moveFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);

    void at_rotationStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void at_rotationFinished(QGraphicsView *view, QGraphicsWidget *widget);

    void at_zoomRectChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect);
    void at_zoomRectCreated(QnMediaResourceWidget *widget, const QColor &color, const QRectF &zoomRect);
    void at_zoomTargetChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect, QnMediaResourceWidget *zoomTargetWidget);

    void at_motionSelectionProcessStarted(QGraphicsView *view, QnMediaResourceWidget *widget);
    void at_motionRegionCleared(QGraphicsView *view, QnMediaResourceWidget *widget);
    void at_motionRegionSelected(QGraphicsView *view, QnMediaResourceWidget *widget, const QRect &region);

    void at_item_leftPressed(QGraphicsView *view, QGraphicsItem *item, ClickInfo info);
    void at_item_leftClicked(QGraphicsView *view, QGraphicsItem *item, ClickInfo info);
    void at_item_rightClicked(QGraphicsView *view, QGraphicsItem *item, ClickInfo info);
    void at_item_middleClicked(QGraphicsView *view, QGraphicsItem *item, ClickInfo info);
    void at_item_doubleClicked(QGraphicsView *view, QGraphicsItem *item, ClickInfo info);
    void at_resourceWidget_doubleClicked(QnResourceWidget *widget);

    void at_scene_clicked(QGraphicsView *view, ClickInfo info);
    void at_scene_doubleClicked(QGraphicsView *view, ClickInfo info);
    void at_scene_keyPressedOrReleased(QGraphicsScene *scene, QEvent *event);
    void at_scene_keyPressed(QGraphicsScene *scene, QEvent *event);
    void at_scene_keyReleased(QGraphicsScene *scene, QEvent *event);
    void at_scene_focusIn(QGraphicsScene *scene, QEvent *event);

    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_display_widgetChanged(Qn::ItemRole role);

    void at_widget_rotationStartRequested(QnResourceWidget *widget);
    void at_widget_rotationStartRequested();
    void at_widget_rotationStopRequested();

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_accessController_permissionsChanged(const QnResourcePtr &resource);

    void at_selectAllAction_triggered();
    void at_startSmartSearchAction_triggered();
    void at_stopSmartSearchAction_triggered();
    void at_toggleSmartSearchAction_triggered();
    void at_clearMotionSelectionAction_triggered();
    void at_toggleInfoAction_triggered();
    void at_maximizeItemAction_triggered();
    void at_unmaximizeItemAction_triggered();
    void at_fitInViewAction_triggered();
    void at_checkFileSignatureAction_triggered();
    void at_nextItemAction_triggered();
    void at_previousItemAction_triggered();
    void at_nextRowAction_triggered();
    void at_previousRowAction_triggered();
    void at_riseCurrentItemAction_triggered();

    void at_zoomedToggle_activated();
    void at_zoomedToggle_deactivated();

    void updateCurrentLayoutInstruments();

    void at_ptzProcessStarted(QnMediaResourceWidget *widget);

private:
    void toggleRaisedState(QnResourceWidget* widget);
    void toggleCurrentItemMaximizationState();

private:
    /* Global state. */

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;

    /** Widgets by role. */
    std::array<QnResourceWidget*, Qn::ItemRoleCount> m_widgetByRole{};

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

    ClickInstrument* m_sceneClickInstrument = nullptr;

    PtzInstrument* m_ptzInstrument = nullptr;

    ZoomWindowInstrument* m_zoomWindowInstrument;

    ObjectTrackingInstrument* m_objectTrackingInstrument;

    GridAdjustmentInstrument* m_gridAdjustmentInstrument;

    bool m_selectionOverlayTuneInstrumentDisabled = false;
    bool m_selectionOverlayTuneInstrumentDisabled2 = false; // TODO: use toggles?


    /* Keyboard control-related state. */

    /** Last keyboard cursor position. */
    QPoint m_cursorPos;

    /** Last item that was affected by keyboard control. */
    QPointer<QnWorkbenchItem> m_cursorItem;


    /* Resizing-related state. */

    /** Widget that is being resized. */
    QnResourceWidget *m_resizedWidget;

    /** Current grid rect of the widget being resized. */
    QRect m_resizedWidgetRect;

    /** Snap point for the current resize process. */
    QPoint m_resizingSnapPoint;


    /* Dragging-related state. */

    /** Workbench items that are being dragged. */
    QList<QnWorkbenchItem*> m_draggedWorkbenchItems;

    /** Workbench items that will be replaced as a result of a drag. */
    QList<QnWorkbenchItem*> m_replacedWorkbenchItems;

    /** Current drag distance. */
    QPoint m_dragDelta;

    /** Target geometries for concatenation of dragged and replaced item lists. */
    QList<QRect> m_dragGeometries;

    bool m_menuEnabled;

    nx::vms::client::desktop::VolatileUniquePtr<QObject> m_cameraSwitchKeysMessageBox;
};
