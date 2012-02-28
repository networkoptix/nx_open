#include "workbench_controller.h"
#include <cassert>
#include <cmath> /* For std::floor. */
#include <limits>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGLWidget>
#include <QGraphicsLinearLayout>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QLabel>
#include <QPropertyAnimation>
#include <QFileInfo>
#include <QSettings>
#include <QFileDialog>
#include <QGraphicsProxyWidget>

#include <utils/common/util.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>

#include <core/resource/resource_directory_browser.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <camera/resource_display.h>
#include <camera/camdisplay.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/preferences/preferencesdialog.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/animation/accessor.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/wheel_zoom_instrument.h>
#include <ui/graphics/instruments/rubber_band_instrument.h>
#include <ui/graphics/instruments/drag_instrument.h>
#include <ui/graphics/instruments/rotation_instrument.h>
#include <ui/graphics/instruments/click_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/stop_accepted_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/selection_fixup_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/resizing_instrument.h>
#include <ui/graphics/instruments/resize_hover_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/selection_overlay_hack_instrument.h>
#include <ui/graphics/instruments/grid_adjustment_instrument.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/grid_item.h>

#include <ui/actions/action_manager.h>

#include <file_processor.h>

#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench_utility.h"
#include "workbench.h"
#include "workbench_display.h"
#include "help/qncontext_help.h"


#define QN_WORKBENCH_CONTROLLER_DEBUG

#ifdef QN_WORKBENCH_CONTROLLER_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

Q_DECLARE_METATYPE(VariantAnimator *)

namespace {
    QAction *newAction(const QString &text, const QString &shortcut, QObject *parent = NULL) {
        QAction *result = new QAction(text, parent);
        result->setShortcut(shortcut);
        return result;
    }

    QRegion createRoundRegion(int rSmall, int rLarge, const QRect &rect) {
        QRegion region;

        int circleX = rLarge;

        int circleY = rSmall-1;
        for (int y = 0; y < qMin(rect.height(), rSmall); ++y)
        {
            // calculate circle Point
            int x = circleX - std::sqrt((double) rLarge*rLarge - (circleY-y)*(circleY-y)) + 0.5;
            region += QRect(x,y, rect.width()-x*2,1);
        }
        for (int y = qMin(rect.height(), rSmall); y < rect.height() - rSmall; ++y)
            region += QRect(0,y, rect.width(),1);

        circleY = rect.height() - rSmall;
        for (int y = rect.height() - rSmall; y < rect.height(); ++y)
        {
            // calculate circle Point
            int x = circleX - std::sqrt((double) rLarge*rLarge - (circleY-y)*(circleY-y)) + 0.5;
            region += QRect(x,y, rect.width()-x*2,1);
        }
        return region;
    }

    QPoint invalidDragDelta() {
        return QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    }

    /** Opacity of video items when they are dragged / resized. */
    const qreal widgetManipulationOpacity = 0.3;

} // anonymous namespace

QnWorkbenchController::QnWorkbenchController(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display),
    m_manager(display->instrumentManager()),
    m_resizedWidget(NULL),
    m_dragDelta(invalidDragDelta())
{
    ::memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    QEvent::Type mouseEventTypeArray[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick,
        QEvent::GraphicsSceneHoverEnter,
        QEvent::GraphicsSceneHoverMove,
        QEvent::GraphicsSceneHoverLeave
    };

    Instrument::EventTypeSet mouseEventTypes = Instrument::makeSet(mouseEventTypeArray);
    Instrument::EventTypeSet wheelEventTypes = Instrument::makeSet(QEvent::GraphicsSceneWheel);
    Instrument::EventTypeSet dndEventTypes = Instrument::makeSet(QEvent::GraphicsSceneDragEnter, QEvent::GraphicsSceneDragMove, QEvent::GraphicsSceneDragLeave, QEvent::GraphicsSceneDrop);

    /* Install and configure instruments. */
    ClickInstrument *itemLeftClickInstrument = new ClickInstrument(Qt::LeftButton, 300, Instrument::ITEM, this);
    m_itemRightClickInstrument = new ClickInstrument(Qt::RightButton, 0, Instrument::ITEM, this);
    ClickInstrument *itemMiddleClickInstrument = new ClickInstrument(Qt::MiddleButton, 0, Instrument::ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(Qt::LeftButton | Qt::RightButton, 0, Instrument::SCENE, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_rotationInstrument = new RotationInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_dropInstrument = new DropInstrument(workbench(), this);
    BoundingInstrument *boundingInstrument = m_display->boundingInstrument();
    SelectionOverlayHackInstrument *selectionOverlayHackInstrument = m_display->selectionOverlayHackInstrument();
    m_dragInstrument = new DragInstrument(this);
    m_itemMouseForwardingInstrument = new ForwardingInstrument(Instrument::ITEM, mouseEventTypes, this);
    SelectionFixupInstrument *selectionFixupInstrument = new SelectionFixupInstrument(this);
    m_motionSelectionInstrument = new MotionSelectionInstrument(this);
    GridAdjustmentInstrument *gridAdjustmentInstrument = new GridAdjustmentInstrument(display->workbench(), this);

    gridAdjustmentInstrument->setSpeed(QSizeF(display->workbench()->mapper()->spacing() / 90.0));

    display->setLayer(m_dropInstrument->surface(), QnWorkbenchDisplay::UI_ELEMENTS_LAYER);

    m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, qnGlobals->motionRubberBandColor());
    m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Border, qnGlobals->motionRubberBandBorderColor());
    m_motionSelectionInstrument->setSelectionModifiers(Qt::ShiftModifier);

    m_rubberBandInstrument->setRubberBandZValue(m_display->layerZValue(QnWorkbenchDisplay::EFFECTS_LAYER));
    m_rotationInstrument->setRotationItemZValue(m_display->layerZValue(QnWorkbenchDisplay::EFFECTS_LAYER));
    m_resizingInstrument->setEffectiveDistance(8);

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(m_resizingInstrument->resizeHoverInstrument());
    m_manager->installInstrument(selectionFixupInstrument);
    m_manager->installInstrument(m_itemMouseForwardingInstrument);
    m_manager->installInstrument(selectionFixupInstrument->preForwardingInstrument());
    m_manager->installInstrument(itemLeftClickInstrument);
    m_manager->installInstrument(m_itemRightClickInstrument);
    m_manager->installInstrument(itemMiddleClickInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, dndEventTypes, this));
    m_manager->installInstrument(m_dropInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, dndEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, dndEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, wheelEventTypes, this));
    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(gridAdjustmentInstrument);

    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, wheelEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, wheelEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, mouseEventTypes, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(m_rotationInstrument, InstallationMode::INSTALL_AFTER, m_display->transformationListenerInstrument());
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_motionSelectionInstrument);

    connect(itemLeftClickInstrument,    SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemLeftClickInstrument,    SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                 this,                           SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_itemRightClickInstrument, SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_rightClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemMiddleClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_middleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *, const ClickInfo &)),                                        this,                           SLOT(at_scene_clicked(QGraphicsView *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *, const ClickInfo &)),                                  this,                           SLOT(at_scene_doubleClicked(QGraphicsView *, const ClickInfo &)));
    connect(m_dragInstrument,           SIGNAL(dragStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               this,                           SLOT(at_dragStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(drag(QGraphicsView *, QList<QGraphicsItem *>)),                                      this,                           SLOT(at_drag(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(dragFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              this,                           SLOT(at_dragFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),          this,                           SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_resizingInstrument,       SIGNAL(resizing(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),                 this,                           SLOT(at_resizing(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),         this,                           SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QnResourceWidget *)),                               this,                           SLOT(at_rotationStarted(QGraphicsView *, QnResourceWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QnResourceWidget *)),                              this,                           SLOT(at_rotationFinished(QGraphicsView *, QnResourceWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                      this,                           SLOT(at_motionSelectionProcessStarted(QGraphicsView *, QnResourceWidget *)));
    connect(m_motionSelectionInstrument,  SIGNAL(motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)),         this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)));
    connect(m_motionSelectionInstrument,  SIGNAL(motionRegionCleared(QGraphicsView *, QnResourceWidget *)),                         this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnResourceWidget *)));

    connect(m_handScrollInstrument,     SIGNAL(scrollStarted(QGraphicsView *)),                                                     boundingInstrument,             SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollFinished(QGraphicsView *)),                                                    boundingInstrument,             SLOT(enforcePosition(QGraphicsView *)));

    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                                  m_wheelZoomInstrument,          SLOT(recursiveDisable()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                                m_wheelZoomInstrument,          SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                                m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                                               m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessStarted(QGraphicsView *)),                                              m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessFinished(QGraphicsView *)),                                             m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessStarted(QGraphicsView *)),                                          m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessFinished(QGraphicsView *)),                                         m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));

    connect(m_dragInstrument,           SIGNAL(dragStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              selectionOverlayHackInstrument, SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandStarted(QGraphicsView *)),                                                 selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandFinished(QGraphicsView *)),                                                selectionOverlayHackInstrument, SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),          selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),         selectionOverlayHackInstrument, SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_rubberBandInstrument,         SLOT(recursiveEnable()));

    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_resizingInstrument,           SLOT(recursiveEnable()));

    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    m_resizingInstrument,           SLOT(recursiveEnable()));

    /* Connect to display. */
    connect(m_display,                  SIGNAL(widgetChanged(QnWorkbench::ItemRole)),                                               this,                           SLOT(at_display_widgetChanged(QnWorkbench::ItemRole)));
    connect(m_display,                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));

    /* Set up context menu. */
    QWidget *window = m_display->view()->window();
    window->addAction(qnAction(Qn::ScreenRecordingAction));

    connect(qnAction(Qn::ShowMotionAction), SIGNAL(triggered()),                                                                    this,                           SLOT(at_showMotionAction_triggered()));
    connect(qnAction(Qn::HideMotionAction), SIGNAL(triggered()),                                                                    this,                           SLOT(at_hideMotionAction_triggered()));
    connect(qnAction(Qn::ScreenRecordingAction), SIGNAL(triggered(bool)),                                                           this,                           SLOT(at_recordingAction_triggered(bool)));

    /* Init screen recorder. */
    m_screenRecorder = new QnScreenRecorder(this);
    if (m_screenRecorder->isSupported()) {
        connect(m_screenRecorder,       SIGNAL(recordingStarted()),                                                                 this,                           SLOT(at_screenRecorder_recordingStarted()));
        connect(m_screenRecorder,       SIGNAL(recordingFinished(QString)),                                                         this,                           SLOT(at_screenRecorder_recordingFinished(QString)));
        connect(m_screenRecorder,       SIGNAL(error(QString)),                                                                     this,                           SLOT(at_screenRecorder_error(QString)));
    }
    m_countdownCanceled = false;
    m_recordingLabel = 0;
    m_recordingAnimation = 0;
}

QnWorkbenchController::~QnWorkbenchController() {
    disconnect(m_screenRecorder, NULL, this, NULL);
    m_screenRecorder->stopRecording();
}

QnWorkbenchDisplay *QnWorkbenchController::display() const {
    return m_display;
}

QnWorkbench *QnWorkbenchController::workbench() const {
    return m_display->workbench();
}

QnWorkbenchGridMapper *QnWorkbenchController::mapper() const {
    return m_display->workbench()->mapper();
}

bool QnWorkbenchController::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Close) {
        if (QnResourceWidget *widget = qobject_cast<QnResourceWidget *>(watched)) {
            /* Clicking on close button of a widget that is not selected should select it,
             * thus clearing the existing selection. */
            if(!widget->isSelected()) {
                display()->scene()->clearSelection();
                widget->setSelected(true);
            }

            qnMenu->trigger(Qn::RemoveLayoutItemAction, display()->scene()->selectedItems());
            event->ignore();
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

void QnWorkbenchController::updateGeometryDelta(QnResourceWidget *widget) {
    if(widget->item()->isPinned())
        return;

    QRectF widgetGeometry = widget->geometry();

    QRectF gridGeometry = mapper()->mapFromGrid(widget->item()->geometry());

    QSizeF step = mapper()->step();
    QRectF geometryDelta = QRectF(
        (widgetGeometry.left()   - gridGeometry.left())   / step.width(),
        (widgetGeometry.top()    - gridGeometry.top())    / step.height(),
        (widgetGeometry.width()  - gridGeometry.width())  / step.width(),
        (widgetGeometry.height() - gridGeometry.height()) / step.height()
    );

    widget->item()->setGeometryDelta(geometryDelta);
}

void QnWorkbenchController::displayMotionGrid(const QList<QnResourceWidget *> &widgets, bool display) {
    foreach(QnResourceWidget *widget, widgets) {
        if(!(widget->resource()->flags() & QnResource::network))
            continue;

        widget->setDisplayFlag(QnResourceWidget::DISPLAY_MOTION_GRID, display);
    }
}


// -------------------------------------------------------------------------- //
// Screen recording
// -------------------------------------------------------------------------- //
void QnWorkbenchController::startRecording()
{
    if (!m_screenRecorder->isSupported()) {
        stopRecording();
        return;
    }

    qnAction(Qn::ScreenRecordingAction)->setChecked(true);

    if(m_screenRecorder->isRecording() || (m_recordingAnimation && m_recordingAnimation->state() == QAbstractAnimation::Running))
        return;

    m_countdownCanceled = false;

    QGLWidget *widget = qobject_cast<QGLWidget *>(display()->view()->viewport());
    if (widget == NULL) {
        qnWarning("Viewport was expected to be a QGLWidget.");
        return;
    }

    QWidget *view = display()->view();

    if (m_recordingLabel == 0)
        m_recordingLabel = new QLabel(view);
    m_recordingLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_recordingLabel->resize(200, 200);
    m_recordingLabel->move(view->mapToGlobal(QPoint(0, 0)) + toPoint(view->size() - m_recordingLabel->size()) / 2);

    m_recordingLabel->setMask(createRoundRegion(18, 18, m_recordingLabel->rect()));
    m_recordingLabel->setText(tr("Recording start in..."));
    m_recordingLabel->setAlignment(Qt::AlignCenter);
    m_recordingLabel->setStyleSheet(QLatin1String("QLabel { font-size:22px; border-width: 2px; border-style: inset; border-color: #535353; border-radius: 18px; background: #212150; color: #a6a6a6; selection-background-color: lightblue }"));
    m_recordingLabel->setFocusPolicy(Qt::NoFocus);
    m_recordingLabel->show();

    if (m_recordingAnimation == 0)
        m_recordingAnimation = new QPropertyAnimation(m_recordingLabel, "windowOpacity", m_recordingLabel);
    m_recordingAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_recordingAnimation->setDuration(3000);
    m_recordingAnimation->setStartValue(1.0);
    m_recordingAnimation->setEndValue(0.6);

    m_recordingAnimation->disconnect();
    connect(m_recordingAnimation, SIGNAL(finished()), this, SLOT(at_recordingAnimation_finished()));
    connect(m_recordingAnimation, SIGNAL(valueChanged(QVariant)), this, SLOT(at_recordingAnimation_valueChanged(QVariant)));
    m_recordingAnimation->start();
}

void QnWorkbenchController::stopRecording()
{
    qnAction(Qn::ScreenRecordingAction)->setChecked(false);

    if (!m_screenRecorder->isSupported())
        return;

    if (!m_screenRecorder->isRecording()) {
        m_countdownCanceled = true;
        return;
    }

    m_screenRecorder->stopRecording();
}

void QnWorkbenchController::at_recordingAnimation_finished()
{
    m_recordingLabel->hide();
    if (!m_countdownCanceled) {
        if (QGLWidget *widget = qobject_cast<QGLWidget *>(display()->view()->viewport()))
            m_screenRecorder->startRecording(widget);
    }
    m_countdownCanceled = false;
}

void QnWorkbenchController::at_recordingAnimation_valueChanged(const QVariant &)
{
    static double TICKS = 3;

    QPropertyAnimation *animation = qobject_cast<QPropertyAnimation *>(sender());
    if (!animation)
        return;

    double normValue = 1.0 - (double) animation->currentTime() / animation->duration();

    QLabel *label = qobject_cast<QLabel *>(animation->targetObject());
    if (!label)
        return;

    if (m_countdownCanceled) {
        label->setText(tr("Cancelled"));
        return;
    }

    double d = normValue * (TICKS+1);
    if (d < TICKS) {
        const int n = int (d) + 1;
        label->setText(tr("Recording start in...") + QString::number(n));
    }
}

void QnWorkbenchController::at_screenRecorder_recordingStarted() {
    return;
}

void QnWorkbenchController::at_screenRecorder_error(const QString &errorMessage) {
    qnAction(Qn::ScreenRecordingAction)->setChecked(false);

    QMessageBox::warning(display()->view(), tr("Warning"), tr("Can't start recording due to following error: %1").arg(errorMessage));
}

void QnWorkbenchController::at_screenRecorder_recordingFinished(const QString &recordedFileName) {
    QString suggetion = QFileInfo(recordedFileName).fileName();
    if (suggetion.isEmpty())
        suggetion = tr("recorded_video");

    QSettings settings;
    settings.beginGroup(QLatin1String("videoRecording"));

    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    QString selectedFilter;
    while (true) {
        QString filePath = QFileDialog::getSaveFileName(
            display()->view(),
            tr("Save Recording As..."),
            previousDir + QLatin1Char('/') + suggetion,
            tr("Transport Stream (*.ts)"),
            &selectedFilter,
            QFileDialog::DontUseNativeDialog
        );

        if (!filePath.isEmpty()) {
            if (!filePath.endsWith(QLatin1String(".ts"), Qt::CaseInsensitive))
                filePath += selectedFilter.mid(selectedFilter.indexOf(QLatin1Char('.')), 3);

            QFile::remove(filePath);
            if (!QFile::rename(recordedFileName, filePath)) {
                QString message = QObject::tr("Can't overwrite file '%1'. Please try another name.").arg(filePath);
                CL_LOG(cl_logWARNING) cl_log.log(message, cl_logWARNING);
                QMessageBox::warning(display()->view(), QObject::tr("Warning"), message, QMessageBox::Ok, QMessageBox::NoButton);
                continue;
            }

            QnFileProcessor::createResourcesForFile(filePath);

            settings.setValue(QLatin1String("previousDir"), QFileInfo(filePath).absolutePath());
        } else {
            QFile::remove(recordedFileName);
        }
        break;
    }
    settings.endGroup();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &) {
    TRACE("RESIZING STARTED");

    m_resizedWidget = qobject_cast<QnResourceWidget *>(item);
    if(m_resizedWidget == NULL)
        return;

    m_display->bringToFront(m_resizedWidget);
    m_display->gridItem()->animatedShow();
    opacityAnimator(m_resizedWidget)->animateTo(widgetManipulationOpacity);
}

void QnWorkbenchController::at_resizing(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &info) {
    if(m_resizedWidget != item)
        return;

    QnResourceWidget *widget = m_resizedWidget;

    QRectF newSceneGeometry = widget->geometry();

    /* Calculate new size in grid coordinates. */
    QSizeF gridSizeF = mapper()->mapToGridF(newSceneGeometry.size());
    QSize gridSize = QSize(
        qMax(1, qRound(gridSizeF.width())),
        qMax(1, qRound(gridSizeF.height()))
    );

    /* Adjust for aspect ratio. */
    if(widget->hasAspectRatio()) {
        if(widget->aspectRatio() > 1.0) {
            gridSize = bestSingleBoundedSize(mapper(), gridSize.width(), Qt::Horizontal, widget->aspectRatio());
        } else {
            gridSize = bestSingleBoundedSize(mapper(), gridSize.height(), Qt::Vertical, widget->aspectRatio());
        }
    }

    /* Calculate new grid rect based on the dragged frame section. */
    QRect newResizingWidgetRect = resizeRect(widget->item()->geometry(), gridSize, info.frameSection());
    if(newResizingWidgetRect != m_resizedWidgetRect) {
        QnWorkbenchLayout::Disposition disposition;
        widget->item()->layout()->canMoveItem(widget->item(), newResizingWidgetRect, &disposition);

        m_display->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::INITIAL);
        m_display->gridItem()->setCellState(disposition.free, QnGridItem::ALLOWED);
        m_display->gridItem()->setCellState(disposition.occupied, QnGridItem::DISALLOWED);

        m_resizedWidgetRect = newResizingWidgetRect;
    }
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &) {
    TRACE("RESIZING FINISHED");

    if(m_resizedWidget != item)
        return;

    QnResourceWidget *widget = m_resizedWidget;

    m_display->gridItem()->animatedHide();
    opacityAnimator(m_resizedWidget)->animateTo(1.0);

    /* Resize if possible. */
    QSet<QnWorkbenchItem *> entities = widget->item()->layout()->items(m_resizedWidgetRect);
    entities.remove(widget->item());
    if (entities.empty()) {
        widget->item()->setGeometry(m_resizedWidgetRect);
        updateGeometryDelta(widget);
    }

    m_display->synchronize(widget->item());

    /* Un-raise the raised item if it was the one being resized. */
    QnWorkbenchItem *raisedItem = workbench()->item(QnWorkbench::RAISED);
    if(raisedItem == widget->item())
        workbench()->setItem(QnWorkbench::RAISED, NULL);

    /* Clean up resizing state. */
    m_display->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::INITIAL);
    m_resizedWidgetRect = QRect();
    m_resizedWidget = NULL;
}

void QnWorkbenchController::at_dragStarted(QGraphicsView *, const QList<QGraphicsItem *> &items) {
    TRACE("DRAG STARTED");

    /* Bring to front preserving relative order. */
    m_display->bringToFront(items);
    m_display->setLayer(items, QnWorkbenchDisplay::FRONT_LAYER);

    /* Show grid. */
    m_display->gridItem()->animatedShow();

    /* Build item lists. */
    foreach (QGraphicsItem *item, items) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        m_draggedWorkbenchItems.push_back(widget->item());

        opacityAnimator(widget)->animateTo(widgetManipulationOpacity);
    }

    /* Un-raise if raised is among the dragged. */
    //QnResourceWidget *raisedWidget = display()->widget(workbench()->raisedItem());
    //if(raisedWidget != NULL && items.contains(raisedWidget))
    //    workbench()->setRaisedItem(NULL);
}

void QnWorkbenchController::at_drag(QGraphicsView *, const QList<QGraphicsItem *> &) {
    if(m_draggedWorkbenchItems.empty())
        return;

    QnWorkbenchLayout *layout = m_draggedWorkbenchItems[0]->layout();

    QPoint newDragDelta = mapper()->mapToGrid(display()->widget(m_draggedWorkbenchItems[0])->geometry()).topLeft() - m_draggedWorkbenchItems[0]->geometry().topLeft();
    if(newDragDelta != m_dragDelta) {
        m_display->gridItem()->setCellState(m_dragGeometries, QnGridItem::INITIAL);

        m_dragDelta = newDragDelta;
        m_replacedWorkbenchItems.clear();
        m_dragGeometries.clear();

        /* Handle single widget case. */
        bool finished = false;
        if(m_draggedWorkbenchItems.size() == 1) {
            QnWorkbenchItem *draggedWorkbenchItem = m_draggedWorkbenchItems[0];

            /* Find item that dragged item was dropped on. */
            QPoint cursorPos = QCursor::pos();
            QSet<QnWorkbenchItem *> replacedWorkbenchItems = layout->items(draggedWorkbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y()));
            if(replacedWorkbenchItems.size() == 1) {
                QnWorkbenchItem *replacedWorkbenchItem = *replacedWorkbenchItems.begin();

                /* Switch places if dropping smaller one on a bigger one. */
                if(replacedWorkbenchItem != NULL && replacedWorkbenchItem != draggedWorkbenchItem && draggedWorkbenchItem->isPinned()) {
                    QSizeF draggedSize = draggedWorkbenchItem->geometry().size();
                    QSizeF replacedSize = replacedWorkbenchItem->geometry().size();
                    if(replacedSize.width() > draggedSize.width() && replacedSize.height() > draggedSize.height()) {
                        QnResourceWidget *draggedWidget = display()->widget(draggedWorkbenchItem);
                        QnResourceWidget *replacedWidget = display()->widget(replacedWorkbenchItem);

                        m_replacedWorkbenchItems.push_back(replacedWorkbenchItem);

                        if(draggedWidget->hasAspectRatio()) {
                            m_dragGeometries.push_back(bestDoubleBoundedGeometry(mapper(), replacedWorkbenchItem->geometry(), draggedWidget->aspectRatio()));
                        } else {
                            m_dragGeometries.push_back(replacedWorkbenchItem->geometry());
                        }

                        if(replacedWidget->hasAspectRatio()) {
                            m_dragGeometries.push_back(bestDoubleBoundedGeometry(mapper(), draggedWorkbenchItem->geometry(), replacedWidget->aspectRatio()));
                        } else {
                            m_dragGeometries.push_back(draggedWorkbenchItem->geometry());
                        }

                        finished = true;
                    }
                }
            }
        }

        /* Handle all other cases. */
        if(!finished) {
            QList<QRect> replacedGeometries;
            foreach (QnWorkbenchItem *workbenchItem, m_draggedWorkbenchItems) {
                QRect geometry = workbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y());
                m_dragGeometries.push_back(geometry);
                if(workbenchItem->isPinned())
                    replacedGeometries.push_back(geometry);
            }

            m_replacedWorkbenchItems = layout->items(replacedGeometries).subtract(m_draggedWorkbenchItems.toSet()).toList();

            replacedGeometries.clear();
            foreach (QnWorkbenchItem *workbenchItem, m_replacedWorkbenchItems)
                replacedGeometries.push_back(workbenchItem->geometry().adjusted(-m_dragDelta.x(), -m_dragDelta.y(), -m_dragDelta.x(), -m_dragDelta.y()));

            m_dragGeometries.append(replacedGeometries);
            finished = true;
        }

        QnWorkbenchLayout::Disposition disposition;
        layout->canMoveItems(m_draggedWorkbenchItems + m_replacedWorkbenchItems, m_dragGeometries, &disposition);

        m_display->gridItem()->setCellState(disposition.free, QnGridItem::ALLOWED);
        m_display->gridItem()->setCellState(disposition.occupied, QnGridItem::DISALLOWED);
    }
}

void QnWorkbenchController::at_dragFinished(QGraphicsView *, const QList<QGraphicsItem *> &) {
    TRACE("DRAG FINISHED");

    if(m_draggedWorkbenchItems.empty())
        return;

    QnWorkbenchLayout *layout = m_draggedWorkbenchItems[0]->layout();

    /* Hide grid. */
    m_display->gridItem()->animatedHide();

    /* Do drag. */
    QList<QnWorkbenchItem *> workbenchItems = m_draggedWorkbenchItems + m_replacedWorkbenchItems;
    bool success = layout->moveItems(workbenchItems, m_dragGeometries);

    foreach(QnWorkbenchItem *item, workbenchItems) {
        QnResourceWidget *widget = display()->widget(item);

        /* Adjust geometry deltas if everything went fine. */
        if(success)
            updateGeometryDelta(widget);

        /* Animate opacity back. */
        opacityAnimator(widget)->animateTo(1.0);
    }

    /* Re-sync everything. */
    foreach(QnWorkbenchItem *workbenchItem, workbenchItems)
        m_display->synchronize(workbenchItem);

    /* Un-raise the raised item if it was among the dragged ones. */
    QnWorkbenchItem *raisedItem = workbench()->item(QnWorkbench::RAISED);
    if(raisedItem != NULL && workbenchItems.contains(raisedItem))
        workbench()->setItem(QnWorkbench::RAISED, NULL);

    /* Clean up dragging state. */
    m_display->gridItem()->setCellState(m_dragGeometries, QnGridItem::INITIAL);
    m_dragDelta = invalidDragDelta();
    m_draggedWorkbenchItems.clear();
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();
}

void QnWorkbenchController::at_rotationStarted(QGraphicsView *, QnResourceWidget *widget) {
    TRACE("ROTATION STARTED");

    m_display->bringToFront(widget);
}

void QnWorkbenchController::at_rotationFinished(QGraphicsView *, QnResourceWidget *widget) {
    TRACE("ROTATION FINISHED");

    if(widget == NULL)
        return; /* We may get NULL if the widget being rotated gets deleted. */

    widget->item()->setRotation(widget->rotation());
}

void QnWorkbenchController::at_motionSelectionProcessStarted(QGraphicsView *, QnResourceWidget *widget) {
    if(!(widget->resource()->flags() & QnResource::network)) {
        m_motionSelectionInstrument->recursiveDisable();
        m_motionSelectionInstrument->recursiveEnable();
        return;
    }

    widget->setDisplayFlag(QnResourceWidget::DISPLAY_MOTION_GRID, true);
    foreach(QnResourceWidget *otherWidget, display()->widgets())
        if(otherWidget != widget)
            otherWidget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionCleared(QGraphicsView *, QnResourceWidget *widget) {
    widget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionSelected(QGraphicsView *, QnResourceWidget *widget, const QRect &region) {
    widget->addToMotionSelection(region);
}

void QnWorkbenchController::at_item_leftClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    TRACE("ITEM LCLICKED");

    if(info.modifiers() != 0)
        return;

    if(workbench()->item(QnWorkbench::ZOOMED) != NULL)
        return; /* Don't change currently raised item if we're zoomed. It is surprising for the user. */

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    QnWorkbenchItem *workbenchItem = widget->item();

    workbench()->setItem(QnWorkbench::RAISED, workbench()->item(QnWorkbench::RAISED) == workbenchItem ? NULL : workbenchItem);
}

void QnWorkbenchController::at_item_rightClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    TRACE("ITEM RCLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    /* Right click does not select items.
     * However, we need to select the item under mouse for the menu to work as expected. */
    if(!widget->isSelected()) {
        widget->scene()->clearSelection();
        widget->setSelected(true);
    }

    QScopedPointer<QMenu> menu(qnMenu->newMenu(Qn::SceneScope, display()->scene()->selectedItems()));
    if(menu->isEmpty())
        return;
    
    menu->exec(info.screenPos());
}

void QnWorkbenchController::at_item_middleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    TRACE("ITEM MCLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    widget->item()->setRotation(0);
}

void QnWorkbenchController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    TRACE("ITEM DOUBLECLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    m_display->scene()->clearSelection();
    widget->setSelected(true);

    QnWorkbenchItem *workbenchItem = widget->item();
    QnWorkbenchItem *zoomedItem = workbench()->item(QnWorkbench::ZOOMED);
    if(zoomedItem == workbenchItem) {
        QRectF viewportGeometry = m_display->viewportGeometry();
        QRectF zoomedItemGeometry = m_display->itemGeometry(zoomedItem);

        if(
            (viewportGeometry.width() < zoomedItemGeometry.width() && !qFuzzyCompare(viewportGeometry.width(), zoomedItemGeometry.width())) ||
            (viewportGeometry.height() < zoomedItemGeometry.height() && !qFuzzyCompare(viewportGeometry.height(), zoomedItemGeometry.height()))
        ) {
            workbench()->setItem(QnWorkbench::ZOOMED, NULL);
            workbench()->setItem(QnWorkbench::ZOOMED, workbenchItem);
        } else {
            workbench()->setItem(QnWorkbench::ZOOMED, NULL);
        }
    } else {
        workbench()->setItem(QnWorkbench::ZOOMED, workbenchItem);
    }
}

void QnWorkbenchController::at_scene_clicked(QGraphicsView *view, const ClickInfo &info) {
    if(info.button() == Qt::LeftButton) {
        at_scene_leftClicked(view, info);
    } else {
        at_scene_rightClicked(view, info);
    }
}

void QnWorkbenchController::at_scene_leftClicked(QGraphicsView *, const ClickInfo &) {
    TRACE("SCENE LCLICKED");

    if(workbench() == NULL)
        return;

    workbench()->setItem(QnWorkbench::RAISED, NULL);
}

void QnWorkbenchController::at_scene_rightClicked(QGraphicsView *, const ClickInfo &info) {
    TRACE("SCENE RCLICKED");

    /* No scene context menu, haha! */
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView *, const ClickInfo &) {
    TRACE("SCENE DOUBLECLICKED");

    if(workbench() == NULL)
        return;

    workbench()->setItem(QnWorkbench::ZOOMED, NULL);
    m_display->fitInView();
}

void QnWorkbenchController::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    QnResourceWidget *widget = m_display->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    if(widget == oldWidget)
        return;

    m_widgetByRole[role] = widget;

    switch(role) {
    case QnWorkbench::ZOOMED: {
        bool effective = widget == NULL;
        m_resizingInstrument->setEffective(effective);
        m_resizingInstrument->resizeHoverInstrument()->setEffective(effective);
        m_dragInstrument->setEffective(effective);

        if(widget == NULL) /* Un-raise on un-zoom. */
            m_display->workbench()->setItem(QnWorkbench::RAISED, NULL);
        break;
    }
    default:
        break;
    }
}

void QnWorkbenchController::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    widget->installEventFilter(this);
}

void QnWorkbenchController::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    widget->removeEventFilter(this);
}

void QnWorkbenchController::at_hideMotionAction_triggered() {
    displayMotionGrid(qnMenu->currentWidgetsTarget(sender()), false);
}

void QnWorkbenchController::at_showMotionAction_triggered() {
    displayMotionGrid(qnMenu->currentWidgetsTarget(sender()), true);
}

void QnWorkbenchController::at_recordingAction_triggered(bool checked) {
    if(checked) {
        startRecording();
    } else {
        stopRecording();
    }
}

