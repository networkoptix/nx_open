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

class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class DragInstrument;
class RubberBandInstrument;
class ResizingInstrument;
class DropInstrument;
class UiElementsInstrument;
class RotationInstrument;
class ClickInfo;

class NavigationItem;

class QnWorkbenchDisplay;
class QnWorkbenchLayout;
class QnWorkbench;
class QnResourceWidget;
class QnWorkbenchItem;
class QnWorkbenchGridMapper;

#ifdef Q_OS_WIN
class QnScreenRecorder;
#endif

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

    void drop(const QUrl &url, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QList<QUrl> &urls, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QString &file, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QList<QString> &files, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QnResourcePtr &resource, const QPoint &gridPos);
    void drop(const QnResourceList &resources, const QPoint &gridPos);

protected:
    void updateGeometryDelta(QnResourceWidget *widget);
    void displayMotionGrid(const QList<QGraphicsItem *> &items, bool display);

protected slots:
    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget);

    void at_dragStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void at_dragFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);

    void at_rotationStarted(QGraphicsView *view, QnResourceWidget *widget);
    void at_rotationFinished(QGraphicsView *view, QnResourceWidget *widget);

    void at_item_clicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_leftClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_rightClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_middleClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void at_item_doubleClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);

    void at_scene_clicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_leftClicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_rightClicked(QGraphicsView *view, const ClickInfo &info);
    void at_scene_doubleClicked(QGraphicsView *view, const ClickInfo &info);

    void at_viewportGrabbed();
    void at_viewportUngrabbed();

    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_display_widgetChanged(QnWorkbench::ItemRole role);

    void at_navigationItem_geometryChanged();

    void at_showMotionAction_triggered();
    void at_hideMotionAction_triggered();

    void at_startRecordingAction_triggered();
    void at_stopRecordingAction_triggered();
    void at_toggleRecordingAction_triggered();
    void at_recordingSettingsActions_triggered();

    void at_screenRecorder_error(const QString &errorMessage);
    void at_screenRecorder_recordingStarted();
    void at_screenRecorder_recordingFinished(const QString &recordedFileName);

    void onPrepareRecording(QVariant value);
    void onRecordingCountdownFinished();
private:
    /** Display synchronizer. */
    QnWorkbenchDisplay *m_display;

    /** Instrument manager for the scene. */ 
    InstrumentManager *m_manager;

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

    /** Navigation item. */
    NavigationItem *m_navigationItem;

    /** Widgets by role. */
    QnResourceWidget *m_widgetByRole[QnWorkbench::ITEM_ROLE_COUNT];


    
    #ifdef Q_OS_WIN
    /** Screen recorder object. */
    QnScreenRecorder *m_screenRecorder;
    #endif

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
    QLabel* m_recordingLabel;

    /** Animation for screen recording countdown. */
    QPropertyAnimation *m_recordingAnimation;
};

#endif // QN_WORKBENCH_CONTROLLER_H
