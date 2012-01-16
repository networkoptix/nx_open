#ifndef QN_WORKBENCH_CONTROLLER_H
#define QN_WORKBENCH_CONTROLLER_H

#include <QObject>
#include <QScopedPointer>
#include <ui/common/scene_utility.h>
#include <core/resource/resource.h>
#include "workbench.h"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QMenu;
class QLabel;
class QPropertyAnimation;
class QGraphicsProxyWidget;

class CLVideoCamera;

class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class DragInstrument;
class RubberBandInstrument;
class ResizingInstrument;
class DropInstrument;
class UiElementsInstrument;
class RotationInstrument;
class MotionSelectionInstrument;
class ClickInfo;
class ResizingInfo;
class VariantAnimator;
class AnimatorGroup;
class HoverProcessor;

class NavigationItem;
class NavigationTreeWidget;

class QnWorkbenchDisplay;
class QnWorkbenchLayout;
class QnWorkbench;
class QnResourceWidget;
class QnWorkbenchItem;
class QnWorkbenchGridMapper;
class QnScreenRecorder;
class QnOpacityHoverItem;
class QnImageButtonWidget;


/**
 * This class implements default scene manipulation logic.
 *
 * It also presents some functions for high-level scene content manipulation.
 */
class QnWorkbenchController: public QObject, protected SceneUtility {
    Q_OBJECT;

public:
    QnWorkbenchController(QnWorkbenchDisplay *display, QObject *parent = NULL);
    virtual ~QnWorkbenchController();

    QnWorkbenchDisplay *display() const;

    QnWorkbench *workbench() const;

    QnWorkbenchLayout *layout() const;

    QnWorkbenchGridMapper *mapper() const;

    NavigationTreeWidget *treeWidget() const {
        return m_treeWidget;
    }

    void drop(const QUrl &url, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QList<QUrl> &urls, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QString &file, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QList<QString> &files, const QPointF &gridPos = QPointF(), bool findAccepted = true);
    void drop(const QnResourcePtr &resource, const QPointF &gridPos = QPointF());
    void drop(const QnResourceList &resources, const QPointF &gridPos = QPointF());

    void remove(const QnResourcePtr &resource);

public Q_SLOTS:
    void setTreeVisible(bool visible, bool animate = true);
    void setSliderVisible(bool visible, bool animate = true);
    void toggleTreeVisible();

protected:
    void updateGeometryDelta(QnResourceWidget *widget);
    void displayMotionGrid(const QList<QGraphicsItem *> &items, bool display);
    int isMotionGridDisplayed();

    VariantAnimator *opacityAnimator(QnResourceWidget *widget);

    void updateViewportMargins();

    void updateTreeGeometry();

protected Q_SLOTS:
    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void at_resizing(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);

    void at_dragStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_drag(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_dragFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);

    void at_rotationStarted(QGraphicsView *view, QnResourceWidget *widget);
    void at_rotationFinished(QGraphicsView *view, QnResourceWidget *widget);

    void at_item_clicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_leftPressed(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
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

    void at_startRecordingAction_triggered();
    void at_stopRecordingAction_triggered();
    void at_toggleRecordingAction_triggered();
    void at_recordingSettingsActions_triggered();

    void at_screenRecorder_error(const QString &errorMessage);
    void at_screenRecorder_recordingStarted();
    void at_screenRecorder_recordingFinished(const QString &recordedFileName);

    void at_recordingAnimation_valueChanged(QVariant value);
    void at_recordingAnimation_finished();

    void at_randomGridAction_triggered();

    void at_controlsWidget_deactivated();
    void at_controlsWidget_geometryChanged();

    void at_navigationItem_geometryChanged();
    void at_navigationItem_actualCameraChanged(CLVideoCamera *camera);

    void at_treeItem_geometryChanged();
    void at_treeHidingProcessor_hoverLeft();
    void at_treeShowingProcessor_hoverEntered();
    void at_treeOpacityProcessor_hoverLeft();
    void at_treeOpacityProcessor_hoverEntered();



private:
    /* Global state. */

    /** Display synchronizer. */
    QnWorkbenchDisplay *m_display;

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;

    /** Navigation item. */
    NavigationItem *m_navigationItem;

    /** Navigation tree widget. */
    NavigationTreeWidget *m_treeWidget;

    /** Proxy widget for navigation tree widget. */
    QGraphicsProxyWidget *m_treeItem;

    QGraphicsWidget *m_treeBackgroundItem;

    /** Graphics widget that triggers tree widget visibility. */
    QGraphicsWidget *m_treeTriggerItem;

    QnImageButtonWidget *m_treeBookmarkItem;

    /** Whether navigation tree is visible. */
    bool m_treeVisible;

    bool m_sliderVisible;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[QnWorkbench::ITEM_ROLE_COUNT];

    /** Hover opacity item for tree widget. */
    HoverProcessor *m_treeHidingProcessor;

    HoverProcessor *m_treeShowingProcessor;

    HoverProcessor *m_treeOpacityProcessor;

    VariantAnimator *m_treeOpacityAnimator;
    VariantAnimator *m_treeBackgroundOpacityAnimator;
    AnimatorGroup *m_treeOpacityAnimatorGroup;
    VariantAnimator *m_treePositionAnimator;
    VariantAnimator *m_sliderPositionAnimator;

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

    /** Ui elements instrument. */
    UiElementsInstrument *m_uiElementsInstrument;

    /** Motion selection instrument. */
    MotionSelectionInstrument *m_motionSelectionInstrument;


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


    /** Screen recorder object. */
    QnScreenRecorder *m_screenRecorder;

    /** Layout item context menu. */
    QMenu *m_itemContextMenu;

    /** Start screen recording action. */
    QAction *m_startRecordingAction;

    /** Stop screen recording action. */
    QAction *m_stopRecordingAction;

    /** Open recording setting dialog. */
    QAction *m_recordingSettingsActions;

    /** Mark recorder countdown is canceled. */
    bool m_countdownCanceled;

    /** Screen recording countdown label. */
    QLabel *m_recordingLabel;

    /** Animation for screen recording countdown. */
    QPropertyAnimation *m_recordingAnimation;

    QAction *m_showMotionAction;
    QAction *m_hideMotionAction;
    QAction *m_randomGridAction;

    QSizeF m_controlsWidgetSize;
};

#endif // QN_WORKBENCH_CONTROLLER_H
